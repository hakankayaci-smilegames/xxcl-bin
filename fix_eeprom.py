import re

with open('xbe-inspect/src/kernel/ex.cpp', 'r') as f:
    content = f.read()

content = content.replace('printf("[KERNEL] ExQueryNonVolatileSetting(Index: %u, Length: %u) called.\n", ValueIndex, ValueLength);', 'printf("[KERNEL] ExQueryNonVolatileSetting(Index: %u, Length: %u) called.\\n", ValueIndex, ValueLength);')
content = content.replace('printf("[KERNEL] ExQueryNonVolatileSetting(Index: %u, Length: %u) called.\\\n", ValueIndex, ValueLength);', 'printf("[KERNEL] ExQueryNonVolatileSetting(Index: %u, Length: %u) called.\\n", ValueIndex, ValueLength);')

with open('xbe-inspect/src/kernel/ex.cpp', 'w') as f:
    f.write(content)
