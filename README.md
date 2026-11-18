Flipper password manager

Save mode:
Emulate storage device and host 1 passwords.txt file
store password 1 in a row in patern `password:@:description/name`

Enter mode:
Choose password by description/name from list, flipper will emulate keyboard and enter password for you

flipper grabs data from emulated file and save is as c structure xored by chacha20 string generated from hash
hash is maked from arrows combo, arrows has data 00, 01, 10, 11 and its combined to generate hash