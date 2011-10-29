top = '.'
out = 'build'

def options(opt):
    opt.load('compiler_cxx')

def configure(cnf):
    cnf.check_tool('compiler_cxx')
    cnf.recurse("main")

def build(bld):    
    voipsteg_inc = bld.path.abspath() + "/include"

    bld.env.includes = [
        voipsteg_inc, 
        "/usr/local/include", 
        "/usr/include/libxml2", 
        "/usr/include/sigc++-2.0/", 
        "/usr/lib/sigc++-2.0/include"
    ]

    bld.recurse("main")
    bld.recurse("lib")

    t = bld(
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

