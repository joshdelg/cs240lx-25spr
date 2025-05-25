# 1-my-elf-loader
How exactly does the .space + b trick work? I know the bootloader tries to start at 0x8000. So, we branch to 0x10000 where our loader actually is instead.
But, isn't that branch instruction vulnerable to being overwritten? Is the assumption that this is fine, because it is the loader which will load the elf
into the overriding space, so we'll never need that 0x8000 branch instruction again?