#!/usr/bin/python3

# coding: utf-8
import os
import argparse
import tempfile
import sys
import math
import zipfile
import logging
import filecmp
import configparser
import re

program_description = \
'''
This program is used to generate a ota.zip

you should make sure you have java environment to run apksigner

<1> If you want to generate a diff ota.zip
    you should use < old bin path > and < new bin path > path to save bin file
    then use \'./gen_ota_zip.py < old bin path > < new bin path >\' to generate a ota.zip
    and ota.sh

<2> If you want to generate a full ota.zip
    you should use < bin path > file to save bin file
    then use \'./gen_ota_zip.py < bin path >\' to generate a ota.zip
    and ota.sh

<3> you can use --output to specify file generation location

<4> the bin name format must be vela_<xxx>.bin
    and in board must use mtd device named /dev/<xxx>
'''

bin_path_help = \
'''
<1> if you input one path,will generate a full ota.zip
<2> if you input two path,will generate a diff ota.zip
'''

patch_path = []
bin_list = []
tools_path=''
speed_dict = {}
logging.basicConfig(format = "[%(levelname)s]%(message)s")
logger = logging.getLogger()

def get_file_size(path):
    stats = os.stat(path)
    return stats.st_size

def parse_speed_conf(args):
    if args.speedconf:
        conf = configparser.ConfigParser()
        conf.read(args.speedconf)
        sections = conf.sections()
        for section in sections:
            items = conf.items(section)
            if (items[0][0] != 'speed' or items[1][0] != 'bin'):
                logger.error("pelase check speed conf file format!")
                exit()
            tmp=re.findall("vela_[a-zA-Z0-9]+.bin",items[1][1])
            for bin in tmp:
                speed_dict[bin] = float(items[0][1])

def gen_diff_ota_sh(patch_path, bin_list, newpartition_list, args, tmp_folder):

    if len(patch_path) == 0 or len(bin_list) == 0:
        logger.error("patch_path or bin_list don't have any file")
        exit(-1)

    bin_list_cnt = len(bin_list)
    fd = open('%s/ota.sh' % (tmp_folder), 'w')

    i = 0
    patch_size_list = []
    while i < bin_list_cnt:
        patch_size_list.append(speed_dict[bin_list[i]] *
                               get_file_size('%s/patch/%spatch' % (tmp_folder, bin_list[i][:-3])))
        i += 1

    i = 0
    bin_size_list = []
    while i < bin_list_cnt:
        bin_size_list.append(speed_dict[bin_list[i]] *
                             get_file_size('%s/%s' % (args.bin_path[1],bin_list[i])))
        i += 1

    for file in newpartition_list:
        bin_size_list.append(speed_dict[file] * get_file_size('%s/%s' % (args.bin_path[1], file)))

    ota_progress = 30.0
    ota_progress_list = []

    i = 0
    while i < bin_list_cnt:
        ota_progress += float(patch_size_list[i] / sum(patch_size_list)) * 30
        ota_progress_list.append(math.floor(ota_progress))
        i += 1

    i = 0
    while i < bin_list_cnt:
        ota_progress += float(bin_size_list[i] / sum(bin_size_list)) * 40
        ota_progress_list.append(math.floor(ota_progress))
        i += 1

    j = 0
    while j < len(newpartition_list):
        ota_progress += float(bin_size_list[i + j] / sum(bin_size_list)) * 40
        ota_progress_list.append(math.floor(ota_progress))
        j += 1

    ota_progress_list[-1] = 100
    str = \
'''set +e
setprop ota.progress.current 30
setprop ota.progress.next %d
''' % (ota_progress_list[0])
    fd.write(str)

    if (args.skip_version_check) :
        str = \
'''setprop ota.version.next `getprop ota.version.current`
'''
    else :
        str = \
'''set version_current `getprop ota.version.current`

echo "new version is "%d

if [ %d -lt $version_current ]
then
    echo "check version failed!"%s
    setprop ota.progress.current -1
    exit
fi
setprop ota.version.next %d
''' % (args.version[0], args.version[0], args.otalog, args.version[0])

    fd.write(str)

    str = \
'''if [ ! -e %s/ota_tmp/%s ]
then
''' % (args.do_ota_path, bin_list[bin_list_cnt - 1])
    fd.write(str)

    i = 0
    while i < bin_list_cnt:

        if bin_list[i] in args.ab:
            patch_tmp = patch_path[i] + '_b'
        else:
            patch_tmp = patch_path[i]

        str = \
'''
    echo "generate %s"%s
    time "bspatch %s %s/ota_tmp/%stmp %s/ota_tmp/%spatch %s"
    if [ $? -ne 0 ]
    then
        echo "bspatch %stmp failed"%s
        setprop ota.progress.current -1
        exit
    fi

    mv %s/ota_tmp/%stmp %s/ota_tmp/%s
    if [ $? -ne 0 ]
    then
        echo "rename %s failed"%s
        setprop ota.progress.current -1
        exit
    fi

    setprop ota.progress.current %d
    setprop ota.progress.next %d
''' % (bin_list[i], args.otalog, patch_tmp, args.do_ota_path, bin_list[i][:-3], args.do_ota_path,
       bin_list[i][:-3], args.patch_compress, bin_list[i][:-3], args.otalog, args.do_ota_path, bin_list[i][:-3],
       args.do_ota_path, bin_list[i], bin_list[i], args.otalog, ota_progress_list[i],
       ota_progress_list[i + 1])
        fd.write(str)
        i += 1

    str = \
'''
fi
'''
    fd.write(str)

    str = \
'''
echo -e -n "a" > %s/ota_tmp/dd
''' % (args.do_ota_path)
    fd.write(str)

    i = 0
    while i < bin_list_cnt:
        str = \
'''
echo "install %s"%s
time "dd if=%s/ota_tmp/%s of=%s bs=%s"
if [ $? -ne 0 ]
then
    echo "dd %s failed"%s
    reboot 1
fi
setprop ota.progress.current %d
'''% (bin_list[i], args.otalog, args.do_ota_path, bin_list[i], patch_path[i], args.bs, bin_list[i], args.otalog, ota_progress_list[bin_list_cnt + i])

        if i + 1 < bin_list_cnt or args.newpartition:
            str += 'setprop ota.progress.next %d\n' % (ota_progress_list[bin_list_cnt + i + 1])
        fd.write(str)
        i += 1

    i = 0
    for file in newpartition_list:
        str = \
'''
echo "install %s"%s
time "dd if=%s/ota_tmp/%s of=%s bs=%s"
if [ $? -ne 0 ]
then
    echo "dd %s failed"%s
    reboot 1
fi
setprop ota.progress.current %d
''' %(file, args.otalog, args.do_ota_path, file,'/dev/' + file[5:-4],
      args.bs, file, args.otalog, ota_progress_list[2 * bin_list_cnt + i])

        if 2 * bin_list_cnt + i < len(ota_progress_list) - 1:
            str += 'setprop ota.progress.next %d\n' % (ota_progress_list[2 * bin_list_cnt + i + 1])
        i += 1
        fd.write(str)

    fd.close()

def gen_diff_ota(args):
    tmp_folder = tempfile.TemporaryDirectory()
    os.makedirs("%s/patch" % (tmp_folder.name), exist_ok = True)

    for old_files in os.walk("%s" % (args.bin_path[0])):pass

    for new_files in os.walk("%s" % (args.bin_path[1])):pass

    ab_flag = False
    if args.ab:
        logger.debug(args.ab)
        for ab_file in args.ab:
            if ab_file not in old_files[2] or ab_file not in new_files[2]:
                logger.error("%s not in %s or %s" % (ab_file, args.bin_path[0], args.bin_path[1]))
                exit(-1)
            if filecmp.cmp("%s/%s" % (args.bin_path[0], ab_file), "%s/%s" % (args.bin_path[1], ab_file)) != True:
                ab_flag = True
    else:
        args.ab = []

    if len(old_files[2]) == 0 or len(new_files[2]) == 0:
        logger.error("No file in the path")
        exit(-1)

    newpartition_list = []
    if args.newpartition:
        newpartition_list = list(set(new_files[2]) - set(old_files[2]))
        for file in newpartition_list:
            if file[0:5] != 'vela_' or file[-4:] != '.bin':
                newpartition_list.remove(file)

    ota_zip = zipfile.ZipFile('%s' % args.output, 'w', compression=zipfile.ZIP_DEFLATED)
    for i in range(len(old_files[2])):
        for j in range(len(new_files[2])):
            oldfile = '%s/%s' % (args.bin_path[0], old_files[2][i])
            newfile = '%s/%s' % (args.bin_path[1], new_files[2][j])
            if old_files[2][i] == new_files[2][j] and \
               old_files[2][i][0:5] == 'vela_' and \
               old_files[2][i][-4:] == '.bin' and \
               (filecmp.cmp(oldfile, newfile) != True or old_files[2][i] in args.ab and ab_flag):
                patchfile = '%s/patch/%spatch' % (tmp_folder.name, new_files[2][j][:-3])
                logger.debug(patchfile)
                ret = os.system("%s/bsdiff %s %s %s %s" % (tools_path, oldfile, newfile,
                                                           patchfile, args.patch_compress))
                if (ret != 0):
                    logger.error("bsdiff error")
                    exit(ret)
                ota_zip.write(patchfile, "%spatch" % new_files[2][j][:-3])
                patch_path.append('/dev/' + old_files[2][i][5:-4])
                bin_list.append(old_files[2][i])

    for file in newpartition_list:
        logger.debug("add %s",file)
        ota_zip.write("%s/%s" % (args.bin_path[1], file), file)
        speed_dict[file] = 1.0

    for file in bin_list:
        speed_dict[file] = 1.0
    parse_speed_conf(args)

    gen_diff_ota_sh(patch_path, bin_list, newpartition_list, args, tmp_folder.name)
    ota_zip.write("%s/ota.sh" % tmp_folder.name, "ota.sh")
    ota_zip.close()

    if args.sign == True:
        n = args.output.rfind('/')
        if n > 0:
            sign_output = args.output[0:n+1] + 'sign_' + args.output[n+1:]
        else:
            sign_output = 'sign_' + args.output
        ret = os.system("java -jar %s/signapk.jar --min-sdk-version 0  %s/%s %s/%s\
                       %s %s" % (tools_path, tools_path, args.cert,
                                 tools_path, args.key, args.output, sign_output))
        if (ret != 0) :
            logger.error("sign error")
            exit(ret)
        logger.info("%s,signature success" % sign_output)

def gen_full_sh(path_list, bin_list, args, tmp_folder):
    path_cnt = len(path_list)
    fd = open('%s/ota.sh' % (tmp_folder),'w')

    i = 0
    size_list = []
    while i < path_cnt:
        size_list.append(speed_dict[bin_list[i]] *
                         get_file_size('%s/%s' % (args.bin_path[0], bin_list[i])))
        i += 1

    ota_progress = 30.0
    ota_progress_list = []

    i = 0
    while i < path_cnt:
        ota_progress += float(size_list[i] / sum(size_list)) * 70
        ota_progress_list.append(math.floor(ota_progress))
        i += 1

    ota_progress_list[-1] = 100
    str = \
'''set +e
setprop ota.progress.current 30
setprop ota.progress.next %d
''' % (ota_progress_list[0])
    fd.write(str)

    if (args.skip_version_check) :
        str = \
'''setprop ota.version.next `getprop ota.version.current`
'''
    else :
        str = \
'''set version_current `getprop ota.version.current`

echo "new version is "%d

if [ %d -lt $version_current ]
then
    echo "check version failed!"%s
    setprop ota.progress.current -1
    exit
fi
setprop ota.version.next %d
''' % (args.version[0], args.version[0], args.otalog, args.version[0])

    fd.write(str)
    # aviod /dev/<xxx> doesn't exist
    i = 0
    while i < path_cnt:
        str = \
'''
if [ ! -e %s ]
then
    echo "%s doesn't exist, will reboot to the old system"%s
    setprop ota.progress.current -1
    exit
fi
''' % (path_list[i], path_list[i], args.otalog)
        fd.write(str)
        i += 1

    str = \
'''
echo -e -n "a" > %s/ota_tmp/dd
''' % (args.do_ota_path)
    fd.write(str)

    i = 0
    while i < path_cnt:
        str =\
'''
echo "install %s"%s
time " dd if=%s/ota_tmp/%s of=%s bs=%s"
if [ $? -ne 0 ]
then
    echo "dd %s failed"%s
    reboot 1
fi
setprop ota.progress.current %d
''' % (bin_list[i], args.otalog, args.do_ota_path, bin_list[i], path_list[i], args.bs, bin_list[i], args.otalog, ota_progress_list[i])
        if i + 1 < path_cnt:
            str += 'setprop ota.progress.next %d\n' % (ota_progress_list[i + 1])
        fd.write(str)
        i += 1

    fd.close()

def gen_full_ota(args):
    tmp_folder = tempfile.TemporaryDirectory()
    for new_files in os.walk("%s" % (args.bin_path[0])):pass

    ota_zip = zipfile.ZipFile('%s' % args.output, 'w', compression=zipfile.ZIP_DEFLATED)
    for i in range(len(new_files[2])):
        if  new_files[2][i][0:5] == 'vela_' and new_files[2][i][-4:] == '.bin':
            newfile = '%s/%s' % (args.bin_path[0], new_files[2][i])
            logger.debug("add %s" % newfile)
            ota_zip.write(newfile, new_files[2][i])
            patch_path.append('/dev/' + new_files[2][i][5:-4])
            bin_list.append(new_files[2][i])

    for file in bin_list:
        speed_dict[file] = 1.0
    parse_speed_conf(args)
    gen_full_sh(patch_path, bin_list, args, tmp_folder.name)

    ota_zip.write("%s/ota.sh" % tmp_folder.name, "ota.sh")
    ota_zip.close()

    if args.sign == True:
        n = args.output.rfind('/')
        if n > 0:
            sign_output = args.output[0:n+1] + 'sign_' + args.output[n+1:]
        else:
            sign_output = 'sign_' + args.output
        ret = os.system("java -jar %s/signapk.jar --min-sdk-version 0  %s/%s %s/%s\
                       %s %s" % (tools_path, tools_path, args.cert,
                                 tools_path, args.key, args.output, sign_output))
        if (ret != 0) :
            logger.error("sign error")
            exit(ret)
        logger.info("%s,signature success" % sign_output)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=program_description,\
                                    formatter_class=argparse.RawTextHelpFormatter)

    parser.add_argument('-k','--key',\
                        help='Private key path,The private key is in pk8 format',\
                        default='keys/key.pk8')

    parser.add_argument('-c','--cert',\
                        help='cert path,The private key is in x509.pem format ',\
                        default='keys/certificate_x509.pem')

    parser.add_argument('--sign',\
                        help='sign ota.zip and be named sign_ota.zip',
                        action='store_true',
                        default=False)

    parser.add_argument('--output',\
                        help='output filepath',\
                        default='ota.zip')

    parser.add_argument('--newpartition',\
                        help='newpartition',
                        action='store_true',
                        default=False)

    parser.add_argument('--bs',\
                        help='ota dd command bs option',\
                        default='32768')

    parser.add_argument('--patch_compress',\
                        help='choose how to compress the patch file. lz4,bz2 or don\'t compress',\
                        choices=['lz4','bz2','none'],\
                        default='lz4')

    parser.add_argument('bin_path',\
                        help=bin_path_help,
                        nargs='*')

    parser.add_argument("--debug", action="store_true",
                        help="print debug log")

    parser.add_argument("--otalog",
                        help="save log /dev/log or a normal file",
                        default='')

    parser.add_argument("--ab",
                        help="mark A/B in diff ota upgrade",
                        nargs='*')

    parser.add_argument("--version",
                        help="set a version number to prevent downgrade",
                        nargs=1,
                        type=int,
                        default=[0])

    parser.add_argument('--skip_version_check',\
                        help='skip version check,all version can update this ota.zip',
                        action='store_true',
                        default=False)

    parser.add_argument("--speedconf",
                        help='''
set speed conf file,this use to control different media progress inconsistencies
conf file like:
[xxx]
speed=<a float num>
bin=<...> (need like vela_<xxx>.bin, support many bins,use "," separated)
example:
[flash]
speed=100.0
bin=vela_ap.bin,vela_test.bin
[sdcrad]
speed=50.0
bin=vela_app.bin,vela_muisc.bin

support many [xxx] to set different speed
if don't have speedconf all bin speed is 1,or not,
will bin size will multiply speed then calculate progress''')

    parser.add_argument('--do_ota_path',\
                        help='set do ota path in device',\
                        default='/data')

    args = parser.parse_args()

    if args.debug:
        logger.setLevel(logging.DEBUG)
    else:
        logger.setLevel(logging.INFO)

    if args.otalog != '':
        args.otalog = ' >> ' + args.otalog

    tools_path = os.path.abspath(os.path.dirname(sys.argv[0]))
    pwd_path = os.getcwd()

    if args.patch_compress == 'none':
        args.patch_compress = ' '

    if os.path.exists(args.output):
        inputstr = input("The %s already exists,will cover it? [Y/N]\n" % args.output)
        if inputstr != 'Y':
            exit()

    if len((args.bin_path)) == 2:
        os.chdir(tools_path)
        if not os.path.exists("bsdiff"):
            os.system('make -C ../../../external/bsdiff/ -f Makefile.host')
            os.system('cp ../../../external/bsdiff/bsdiff .')
            os.system('make -C ../../../external/bsdiff/ -f Makefile.host clean')
        os.chdir(pwd_path)
        gen_diff_ota(args)
    elif len(args.bin_path) == 1:
        gen_full_ota(args)
    else:
        parser.print_help()
