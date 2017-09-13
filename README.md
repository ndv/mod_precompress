# mod_precompress
Serve precompressed .gz files in Apache2. If the browser does not support gzip compression, the .gz files are decompressed and served as-is.

## Install on Linux

* Install the apache dev package; on linux:
```
sudo apt-get install apache2-dev
```
* Make sure the mod_deflate module is enabled: 
```
ls /etc/apache2/mods-enabled/deflate.load
```
* Compile & install the module:
```
sudo apxs -cia mod_precompress.c
```
## Install on Windows
* Copy the corresponding pre-built 32- or 64- bit library from [win]win to apache/modules
* Add the following line in httpd.conf or apache2.conf:
```apache
LoadModule precompress_module modules/mod_precompress.so
```

## Usage

* Enable precompression in the /etc/apache2/apache2.conf, in the `<Directory ...>` section:
```apache
<Directory />
    AllowOverride none
    Require all denied
    Precompressed on
</Directory>
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
