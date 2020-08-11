# Windows-Hardware-Id
A solution to get unique hardware identifiers on a Windows PC

Retrieves the following:

System Management BIOS (SMBIOS)
-------------
Manufacturer
Product
Version
Serial Number
SKU Number
Family

Disk
-------------
Serial Number
Vendor
Product

CPU
-------------
Processor Features (raw and hashed)
Architecture
Level
Active Processor Mask

Windows
-------------
Machine GUID
Computer Hardware Id
SQM Client Machine Id
Build Lab
Install Time
Install Date
Build GUID


A unique hash is created from the disk serial number, processor features, processor archetecture, SMBIOS manufacturer, and SMBIOS product.
