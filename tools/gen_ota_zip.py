#!/usr/bin/python3

# coding: utf-8
import os
import argparse
import tempfile
import math

program_description = \
'''
This program is used to genrate a ota.zip

you should make sure you have java environment to run apksigner

<1> If you want generate a diff ota.zip
    you should use < old bin path > and < new bin path > path to sava bin file
    then use \'./gen_ota_zip.py < old bin path > < new bin path >\' to genrate a ota.zip
    and ota.sh

<2>If you want generate a full ota.zip
    you should use < bin path > file to sava bin file
    then use \'./gen_ota_zip.py < bin path >\' to genrate a ota.zip
    and ota.sh

<3> you can use --output to specify file generation location

<4> 
    the bin name format must be vela_<xxx>.bin
    and in borad must use mtd device named /dev/<xxx>
'''

bin_path_help = \
'''
<1> if you input one path,will genrate a full ota.zip
<2> if you input two path,will genrate a diff ota.zip
'''

patch_path = []
bin_list = []

def get_file_size(path):
    stats = os.stat(path)
    return stats.st_size

def gen_diff_ota_sh(patch_path, bin_list, args, tmp_folder):

    bin_list_cnt = len(bin_list)
    fd = open('%s/ota.sh' % (tmp_folder), 'w')

    i = 0
    patch_size_list = []
    while i < bin_list_cnt:
        patch_size_list.append(get_file_size('%s/patch/%spatch' % (tmp_folder, bin_list[i][:-3])))
        i += 1

    i = 0
    bin_size_list = []
    while i < bin_list_cnt:
        bin_size_list.append(get_file_size('%s/%s' % (args.bin_path[1],
                                                         bin_list[i])))
        i += 1

    if args.newpartition:
        bin_size_list.append(get_file_size('%s/%s' % (args.bin_path[1], args.newpartition)))

    ota_progress = 30.0
    ota_progress_list = []

    i = 0
    while i < bin_list_cnt:
        ota_progress += float(patch_size_list[i] / sum(patch_size_list)) * 30
        ota_progress_list.append(round(ota_progress))
        i += 1

    i = 0
    while i < bin_list_cnt:
        ota_progress += float(bin_size_list[i] / sum(bin_size_list)) * 40
        ota_progress_list.append(round(ota_progress))
        i += 1

    if args.newpartition:
        ota_progress += float(bin_size_list[bin_list_cnt + 1] / sum(bin_size_list)) * 40
        ota_progress_list.append(round(ota_progress))

    str = \
'''set +e
setprop ota.progress.current 30
setprop ota.progress.next %d
''' % (ota_progress_list[0])
    fd.write(str)

    str = \
'''if [ ! -e /data/ota_tmp/%s ]
then
''' % (bin_list[bin_list_cnt - 1])
    fd.write(str)

    i = 0
    while i < bin_list_cnt:
        str = \
'''
    echo "genrate %s"
    time "bspatch %s /data/ota_tmp/%stmp /data/ota_tmp/%spatch"
    if [ $? -ne 0 ]
    then
        echo "bspatch %stmp failed"
        exit
    fi

    mv /data/ota_tmp/%stmp /data/ota_tmp/%s
    if [ $? -ne 0 ]
    then
        echo "rename %s failed"
        exit
    fi

    setprop ota.progress.current %d
    setprop ota.progress.next %d
''' % (bin_list[i], patch_path[i], bin_list[i][:-3],
       bin_list[i][:-3], bin_list[i][:-3], bin_list[i][:-3],
       bin_list[i], bin_list[i], ota_progress_list[i],
       ota_progress_list[i + 1])
        fd.write(str)
        i += 1

    str = \
'''
fi
'''
    fd.write(str)
    i = 0
    while i < bin_list_cnt:
        str = \
'''
echo "install %s"
time "dd if=/data/ota_tmp/%s of=%s bs=%s"
if [ $? -ne 0 ]
then
    echo "dd %s failed"
    reboot 1
fi
setprop ota.progress.current %d
'''% (bin_list[i], bin_list[i], patch_path[i], args.bs, bin_list[i], ota_progress_list[bin_list_cnt + i])

        if i + 1 < bin_list_cnt or args.newpartition:
            str += 'setprop ota.progress.next %d\n' % (ota_progress_list[bin_list_cnt + i + 1])
        fd.write(str)
        i += 1

    if args.newpartition:
        str = \
'''
echo "install %s"
time "dd if=/data/ota_tmp/%s of=%s bs=%s"
if [ $? -ne 0 ]
then
    echo "dd %s failed"
    reboot 1
fi
setprop ota.progress.current %d
''' %(args.newpartition, args.newpartition,'/dev/' + args.newpartition[5:-4],
      args.bs, args.newpartition, ota_progress_list[2 * bin_list_cnt + 1])
        fd.write(str)

    fd.close()

def gen_diff_ota(args):
    tmp_folder = tempfile.TemporaryDirectory()
    os.makedirs("%s/patch" % (tmp_folder.name), exist_ok = True)

    for old_files in os.walk("%s" % (args.bin_path[0])):pass

    for new_files in os.walk("%s" % (args.bin_path[1])):pass

    if 'vela_ota.bin' in old_files[2]:
        old_files[2].remove('vela_ota.bin')

    if 'vela_ota.bin' in new_files[2]:
        new_files[2].remove('vela_ota.bin')

    if len(old_files[2]) == 0 or len(new_files[2]) == 0:
        print("No file in the path")
        exit(-1)

    if args.newpartition:
        if args.newpartition not in new_files[2]:
            print("pelse check you new path and new partion name")
            exit(-1)
        if args.newpartition[0:5] != 'vela_' or \
           args.newpartition[-4:] != '.bin':
            print('please cheak new partion name')
            exit(-1)

    for i in range(len(old_files[2])):
        for j in range(len(new_files[2])):
            if old_files[2][i] == new_files[2][j] and \
               old_files[2][i][0:5] == 'vela_' and \
               old_files[2][i][-4:] == '.bin':
                print(old_files[2][i])
                oldfile = '%s/%s' % (args.bin_path[0], old_files[2][i])
                newfile = '%s/%s' % (args.bin_path[1], new_files[2][j])
                patchfile = '%s/patch/%spatch' % (tmp_folder.name, new_files[2][j][:-3])
                print(patchfile)
                ret = os.system("./bsdiff %s %s %s" % (oldfile, newfile, patchfile))
                if (ret != 0):
                    print("bsdiff error")
                    exit(ret)
                os.system("zip -j -1 %s %s" % (args.output, patchfile))
                patch_path.append('/dev/' + old_files[2][i][5:-4])
                bin_list.append(old_files[2][i])

    if args.newpartition:
        os.system("zip -j -1 %s %s/%s" % (args.output, args.bin_path[1],args.newpartition))

    gen_diff_ota_sh(patch_path, bin_list, args, tmp_folder.name)
    os.system("zip -j -1 %s %s/ota.sh" % (args.output, tmp_folder.name))

    ret = os.system("java -jar apksigner.jar sign --key %s --cert %s\
                 --min-sdk-version 0 %s" % (args.key, args.cert, args.output))
    if (ret != 0) :
        print("apksigner error")
        exit(ret)
    print("ota.zip signature success")

def gen_full_sh(path_list, bin_list, args, tmp_folder):
    path_cnt = len(path_list)
    fd = open('%s/ota.sh' % (tmp_folder),'w')

    i = 0
    size_list = []
    while i < path_cnt:
        size_list.append(get_file_size('%s/%s' % (args.bin_path[0],
                                                         bin_list[i])))
        i += 1

    ota_progress = 30.0
    ota_progress_list = []

    i = 0
    while i < path_cnt:
        ota_progress += float(size_list[i] / sum(size_list)) * 70
        ota_progress_list.append(round(ota_progress))
        i += 1

    str = \
'''set +e
setprop ota.progress.current 30
setprop ota.progress.next %d
''' % (ota_progress_list[0])
    fd.write(str)

    i = 0
    while i < path_cnt:
        str =\
'''
echo "install %s"
time " dd if=/data/ota_tmp/%s of=%s bs=%s"
if [ $? -ne 0 ]
then
    echo "dd %s failed"
    reboot 1
fi
setprop ota.progress.current %d
''' % (bin_list[i], bin_list[i], path_list[i], args.bs, bin_list[i], ota_progress_list[i])
        if i + 1 < path_cnt:
            str += 'setprop ota.progress.next %d\n' % (ota_progress_list[i + 1])
        fd.write(str)
        i += 1

    fd.close()

def gen_full_ota(args):
    tmp_folder = tempfile.TemporaryDirectory()
    for new_files in os.walk("%s" % (args.bin_path[0])):pass

    if 'vela_ota.bin' in new_files[2]:
        new_files[2].remove('vela_ota.bin')

    if len(new_files[2]) == 0:
        print("No file in the path")
        exit(-1)

    for i in range(len(new_files[2])):
        if  new_files[2][i][0:5] == 'vela_' and new_files[2][i][-4:] == '.bin':
            newfile = '%s/%s' % (args.bin_path[0], new_files[2][i])
            os.system("zip -j -1 %s %s" % (args.output, newfile))
            patch_path.append('/dev/' + new_files[2][i][5:-4])
            bin_list.append(new_files[2][i])

    gen_full_sh(patch_path, bin_list, args, tmp_folder.name)

    os.system("zip -j -1 %s %s/ota.sh" % (args.output, tmp_folder.name))

    ret = os.system("java -jar apksigner.jar sign --key %s --cert %s\
                 --min-sdk-version 0 %s" % (args.key, args.cert, args.output))
    if (ret != 0) :
        print("apksigner error")
        exit(ret)
    print("ota.zip signature success")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=program_description,\
                                    formatter_class=argparse.RawTextHelpFormatter)

    parser.add_argument('-k','--key',\
                        help='Private key path,The private key is in pk8 format',\
                        default='keys/key.pk8')

    parser.add_argument('-c','--cert',\
                        help='cert path,The private key is in x509.pem format ',\
                        default='keys/certificate_x509.pem')

    parser.add_argument('--output',\
                        help='output filepath',\
                        default='ota.zip')

    parser.add_argument('--newpartition',\
                        help='newpartition')

    parser.add_argument('--bs',\
                        help='ota dd command bs option',\
                        default='32768')

    parser.add_argument('bin_path',\
                        help=bin_path_help,
                        nargs='*')

    args = parser.parse_args()

    if os.path.exists(args.output):
        os.system("rm %s" % (args.output))

    if len((args.bin_path)) == 2:
        gen_diff_ota(args)
    elif len(args.bin_path) == 1:
        gen_full_ota(args)
    else:
        parser.print_help()
