#!/usr/bin/env python

# coding: utf-8
import sys
import os
def help():
    print("./gen_ota_zip oldfile newfile ota.zip")

def print_file_size(path):
    stats = os.stat(path);
    print("%s size: %d byte" % (path,float(stats.st_size) ))

def compressrate(newfilepath,otapath):
      newfile_stas = os.stat(newfilepath)
      otapath_stats =  os.stat(otapath)
      rate = float(otapath_stats.st_size) / float(newfile_stas.st_size)

      print("ota.zip divide newfile is %.2f%%" % (rate * 100))

if __name__=="__main__":

    if (len(sys.argv) < 3):
        help()
        exit()
    print_file_size(sys.argv[1])
    print_file_size(sys.argv[2])
    old = open(sys.argv[1], 'rb')
    old_stats = os.stat(sys.argv[1])
    new = open(sys.argv[2], 'rb')
    new_stats  = os.stat(sys.argv[2])

    old0 = open("old0", 'wb')
    data = old.read(old_stats.st_size - 4)
    old0.write(data)
    old0.close()

    new0 = open("new0", 'wb')
    data = new.read(new_stats.st_size - 4)
    new0.write(data)
    new0.close()

    ret = os.system("./bsdiff old0 new0 patch.bin")
    if (ret != 0) :
        print("bidiff error")
        exit(ret)
    print_file_size("patch.bin")

    ret = os.system("zip -j -1 %s \
            ../../../vendor/bes/boards/best1600_ep/src/etc/ota.sh patch.bin" % (sys.argv[3]))
    if (ret != 0) :
        print("zip ota.zip error")
        exit(ret)
    ret = os.system("apksigner sign --key keys/key.pk8 --cert keys/certificate_x509.pem\
                 --min-sdk-version 0 %s" % (sys.argv[3]))
    if (ret != 0) :
        print("apksigner error")
        exit(ret)
    print("ota.zip signature success")
    print_file_size(sys.argv[3])