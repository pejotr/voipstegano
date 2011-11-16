#! /usr/bin/env python
# encoding: utf-8

top = '.'
out = 'build'

def options(opt):
    opt.tool_options('compiler_cxx')

def configure(cnf):
    cnf.check_tool('compiler_cxx')
    cnf.recurse("main")

    cnf.setenv('sender', env = cnf.env.derive())
    cnf.load('compiler_cxx')
    cnf.env.CXXFLAGS = ['-g3', '-gdwarf-2']
    cnf.define("MODULE_SENDER", 1)
    cnf.write_config_header("sender/config.h")

    cnf.setenv('receiver', env = cnf.env.derive())
    cnf.load('compiler_cxx')
    cnf.define("MODULE_RECEIVER", 1)
    cnf.write_config_header("receiver/config.h")


def build(bld):    
    voipsteg_inc = bld.path.abspath() + "/include"

    print(bld.env) 

    bld.env.includes = [
        ".",
        voipsteg_inc, 
        "/usr/local/include", 
        "/usr/include/libxml2", 
        "/usr/include/sigc++-2.0/", 
        "/usr/lib/sigc++-2.0/include"
    ]

    bld.recurse("main")
    bld.recurse("lib")

    t = bld.program(
        features = "cxx cxxprogram",
        source   = "main.cpp",
        target   = "testmain",
        includes = bld.env.includes,
        lib      = ["pthread"],
        use      = [
            "sip", 
            "rtp", 
            "tools",
            "algo",
            "exttools",
            "libpjproject", 
            "libnetfilter_queue",
            "libsigc++",
            "libxml2"
        ]
    )


def init(ctx):
    from waflib.Build import BuildContext, CleanContext, InstallContext, UninstallContext
    for x in 'sender receiver'.split():
        for y in (BuildContext, CleanContext, InstallContext, UninstallContext):
            name = y.__name__.replace('Context','').lower()
            class tmp(y):
                cmd = name + '_' + x
                variant = x

