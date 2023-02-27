#!/usr/bin/python

import os, re

def mkdefs(hw):

    f = open('hal/hw/' + hw + '.h', 'r')
    g = open('hal/mk/' + hw + '.d', 'w')

    for s in f:
        if   'HW_MCU_STM32F405' in s:
            g.write('HWMCU = STM32F405\n')
        elif 'HW_MCU_STM32F722' in s:
            g.write('HWMCU = STM32F722\n')

        elif 'HW_HAVE_DRV_ON_PCB' in s:
            g.write('OBJ_HAL_DRV = INCLUDE\n')

        elif 'HW_HAVE_USB_CDC_ACM' in s:
            g.write('OBJ_HAL_USB = INCLUDE\n')
            g.write('OBJ_LIB_CHERRY = INCLUDE\n')

        elif 'HW_HAVE_NETWORK_EPCAN' in s:
            g.write('OBJ_HAL_CAN = INCLUDE\n')
            g.write('OBJ_EPCAN = INCLUDE\n')

    f.close()
    g.close()

def mkbuild():

    for file in os.listdir('hal/hw'):
        if file.endswith(".h"):
            mkdefs(file[0:-2])

def checkmacro(s, m):

    m = re.search('^\s*' + m + '\(.+\)', s)
    return True if m != None else False

def shdefs(file, g):

    f = open(file, 'r')

    if file.endswith('epcan.c'):
        g.write('#ifdef HW_HAVE_NETWORK_EPCAN\n')

    for s in f:
        if checkmacro(s, 'SH_DEF'):
            m = re.search('\(\w+\)', s).group(0)
            s = re.sub('[\s\(\)]', '', m)
            g.write('SH_DEF(' + s + ')\n')

    if file.endswith('epcan.c'):
        g.write('#endif /* HW_HAVE_NETWORK_EPCAN */\n')

    f.close()

def shbuild():

    g = open('shdefs.h', 'w')

    for path in ['./', 'app/', 'hal/']:
        for file in os.listdir(path):
            if file.endswith(".c"):
                shdefs(path + file, g)

    g.close()

def apbuild():

    g = open('app/apdefs.h', 'w')

    for file in os.listdir('app/'):
        if file.endswith(".c"):
            s = file.removesuffix('.c')
            g.write('APP_DEF(' + s.upper() + ')\n')

def regdefs():

    distance = 10

    f = open('regfile.c', 'r')
    g = open('regdefs.h', 'w')

    for s in f:
        if s[0] == '#':
            if distance < 4:
                g.write(s)
        elif checkmacro(s, 'REG_DEF'):
            s = re.search('\([\w\.]+?,\s*[\w\.]*?,', s).group(0)
            s = re.sub('[\(\,\s]', '', s).replace('.', '_')
            g.write('ID_' + s.upper() + ',\n')
            distance = 0

        distance += 1

    f.close()
    g.close()

mkbuild()
shbuild()
apbuild()
regdefs()
