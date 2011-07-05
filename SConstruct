import os

#BuilDir('build', 'main')
env = Environment(CC = 'g++')

#env.Append(CPPPATH = ['/home/pejotrdev/workshop/inz/inz-voipstegano/include/'])

#sources = Split(""" main/utils.c """)
#object_list = env.Object(source = sources)

SConscript('main/SConscript', variant_dir='build', duplicate = 0)
SConscript('lib/SConscript', variant_dir='build/lib')
#SConscript('main/sip/SConscript', variant_dir='build/sip')
#SConscript('main/rtp/SConscript', variant_dir='build/rtp')

# Run all test scripts 
path = "tests"
dir_list = os.listdir(path)

for dir in dir_list:
    sconscript_path = os.path.join('tests', dir, 'SConscript')
    if ( os.path.exists(sconscript_path) ) :
        print "Parsing " + sconscript_path
        SConscript( os.path.join('tests/', dir, 'SConscript'))
    else :
        print "[!] Missing file " + sconscript_path

