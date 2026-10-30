import re

def fix_file(filename):
    with open(filename, 'r') as f:
        content = f.read()
    
    # Fix broken string literals
    content = content.replace('(Tracked)\n",', '(Tracked)\\n",')
    content = content.replace('(Fallback)\n",', '(Fallback)\\n",')
    
    with open(filename, 'w') as f:
        f.write(content)

fix_file('xbe-inspect/src/kernel/mm.cpp')
fix_file('xbe-inspect/src/kernel/ex.cpp')
