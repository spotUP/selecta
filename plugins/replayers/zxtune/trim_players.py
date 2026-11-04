import re, sys
root = sys.argv[1]
p = root + '/src/core/plugins/players/plugins_list.cpp'
s = open(p).read()
keep = ['AYMPlugins', 'DACPlugins', 'TFMPlugins', 'COPSupport', 'MTCSupport']
out = []
for l in s.split('\n'):
    m = re.match(r'\s*(?:void )?Register(\w+)\(players', l)
    if m and m.group(1) not in keep:
        continue
    out.append(l)
open(p, 'w').write('\n'.join(out))
p = root + '/src/core/plugins/players/Makefile'
s = open(p).read().replace('source_dirs := . ay dac external multi saa tfm xsf', 'source_dirs := . ay dac multi saa tfm')
open(p, 'w').write(s)
