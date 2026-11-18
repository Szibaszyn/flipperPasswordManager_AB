#include "mass_storage_scsi.h"
#include <furi.h>
#include <string.h>

#define TAG "SCSI"

#define SCSI_TEST_UNIT_READY        0x00
#define SCSI_REQUEST_SENSE          0x03
#define SCSI_INQUIRY                0x12
#define SCSI_MODE_SENSE_6           0x1A
#define SCSI_START_STOP_UNIT        0x1B
#define SCSI_PREVENT_ALLOW_MEDIUM   0x1E
#define SCSI_READ_FORMAT_CAPACITIES 0x23
#define SCSI_READ_CAPACITY_10       0x25
#define SCSI_READ_10                0x28
#define SCSI_WRITE_10               0x2A

bool scsi_cmd_start(SCSISession* scsi, uint8_t* cmd, uint8_t len) {
    if(!len) {
        scsi->sk = 0x05; // Illegal request
        scsi->asc = 0x20;
        return false;
    }
    scsi->cmd = cmd;
    scsi->cmd_len = len;
    scsi->rx_done = false;
    scsi->tx_done = false;

    if(cmd[0] == SCSI_WRITE_10) {
        if(len < 10) return false;
        scsi->write_10.lba = ((uint32_t)cmd[2] << 24) | ((uint32_t)cmd[3] << 16) |
                             ((uint32_t)cmd[4] << 8) | ((uint32_t)cmd[5]);
        scsi->write_10.count = ((uint16_t)cmd[7] << 8) | ((uint16_t)cmd[8]);
        return true;
    } else if(cmd[0] == SCSI_READ_10) {
        if(len < 10) return false;
        scsi->read_10.lba = ((uint32_t)cmd[2] << 24) | ((uint32_t)cmd[3] << 16) |
                            ((uint32_t)cmd[4] << 8) | ((uint32_t)cmd[5]);
        scsi->read_10.count = ((uint16_t)cmd[7] << 8) | ((uint16_t)cmd[8]);
        return true;
    }
    return true;
}

bool scsi_cmd_rx_data(SCSISession* scsi, uint8_t* data, uint32_t len) {
    if(scsi->rx_done || scsi->cmd[0] != SCSI_WRITE_10) return false;

    uint16_t blocks = len / SCSI_BLOCK_SIZE;
    bool ok = scsi->fn.write(scsi->fn.ctx, scsi->write_10.lba, blocks, data, len);
    scsi->write_10.lba += blocks;
    scsi->write_10.count -= blocks;
    if(!scsi->write_10.count) {
        scsi->rx_done = true;
    }
    return ok;
}

bool scsi_cmd_tx_data(SCSISession* scsi, uint8_t* data, uint32_t* len, uint32_t cap) {
    if(scsi->tx_done) return false;

    switch(scsi->cmd[0]) {
    case SCSI_REQUEST_SENSE: {
        if(cap < 18) return false;
        memset(data, 0, 18);
        data[0] = 0x70;
        data[2] = scsi->sk;
        data[7] = 10;
        data[12] = scsi->asc;
        *len = 18;
        scsi->sk = 0;
        scsi->asc = 0;
        scsi->tx_done = true;
        return true;
    }
    case SCSI_INQUIRY: {
        if(cap < 36) return false;
        memset(data, 0, 36);
        data[0] = 0x00; // Direct access device
        data[1] = 0x80; // Removable
        data[2] = 0x02; // ANSI SCSI-2
        data[3] = 0x02; // Response data format
        data[4] = 31;
        memcpy(data + 8, "Flipper ", 8);
        memcpy(data + 16, "PassManager Disk", 16);
        memcpy(data + 32, "1.00", 4);
        *len = 36;
        scsi->tx_done = true;
        return true;
    }
    case SCSI_READ_FORMAT_CAPACITIES: {
        if(cap < 12) return false;
        uint32_t blocks = scsi->fn.num_blocks(scsi->fn.ctx);
        memset(data, 0, 12);
        data[3] = 8;
        data[4] = (uint8_t)(blocks >> 24);
        data[5] = (uint8_t)(blocks >> 16);
        data[6] = (uint8_t)(blocks >> 8);
        data[7] = (uint8_t)(blocks);
        data[8] = 0x02; // Formatted media
        data[9] = (uint8_t)(SCSI_BLOCK_SIZE >> 16);
        data[10] = (uint8_t)(SCSI_BLOCK_SIZE >> 8);
        data[11] = (uint8_t)(SCSI_BLOCK_SIZE);
        *len = 12;
        scsi->tx_done = true;
        return true;
    }
    case SCSI_READ_CAPACITY_10: {
        if(cap < 8) return false;
        uint32_t blocks = scsi->fn.num_blocks(scsi->fn.ctx);
        uint32_t lba = (blocks > 0) ? (blocks - 1) : 0;
        data[0] = (uint8_t)(lba >> 24);
        data[1] = (uint8_t)(lba >> 16);
        data[2] = (uint8_t)(lba >> 8);
        data[3] = (uint8_t)(lba);
        data[4] = (uint8_t)(SCSI_BLOCK_SIZE >> 24);
        data[5] = (uint8_t)(SCSI_BLOCK_SIZE >> 16);
        data[6] = (uint8_t)(SCSI_BLOCK_SIZE >> 8);
        data[7] = (uint8_t)(SCSI_BLOCK_SIZE);
        *len = 8;
        scsi->tx_done = true;
        return true;
    }
    case SCSI_MODE_SENSE_6: {
        if(cap < 4) return false;
        memset(data, 0, 4);
        data[0] = 3;
        *len = 4;
        scsi->tx_done = true;
        return true;
    }
    case SCSI_READ_10: {
        bool ok = scsi->fn.read(scsi->fn.ctx, scsi->read_10.lba, scsi->read_10.count, data, len, cap);
        uint16_t blocks = *len / SCSI_BLOCK_SIZE;
        scsi->read_10.lba += blocks;
        scsi->read_10.count -= blocks;
        if(!scsi->read_10.count) {
            scsi->tx_done = true;
        }
        return ok;
    }
    default:
        scsi->sk = 0x05;
        scsi->asc = 0x20;
        return false;
    }
}

bool scsi_cmd_end(SCSISession* scsi) {
    uint8_t op = scsi->cmd ? scsi->cmd[0] : 0;
    scsi->cmd = NULL;
    scsi->cmd_len = 0;

    switch(op) {
    case SCSI_WRITE_10: return scsi->rx_done;
    case SCSI_READ_10:
    case SCSI_REQUEST_SENSE:
    case SCSI_INQUIRY:
    case SCSI_READ_FORMAT_CAPACITIES:
    case SCSI_READ_CAPACITY_10:
    case SCSI_MODE_SENSE_6:
        return scsi->tx_done;
    case SCSI_TEST_UNIT_READY:
    case SCSI_PREVENT_ALLOW_MEDIUM:
        return true;
    case SCSI_START_STOP_UNIT:
        if(scsi->fn.eject) scsi->fn.eject(scsi->fn.ctx);
        return true;
    default:
        return false;
    }
}
