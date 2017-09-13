# mod_precompress
Serve precompressed .gz files in Apache2. If the browser does not support gzip compression, the .gz files are decompressed and served as-is.

## Usage:

* Install apache dev package; on linux:
```
sudo apt-get install apache2-dev
```
* Compile & install the module:
```
sudo apxs -cia mod_precompress.c
```
* Restart Apache:
```
sudo service apache2 restart
```
* Compress static data:
```
cd /var/www/htdocs
gzip index.html
```
