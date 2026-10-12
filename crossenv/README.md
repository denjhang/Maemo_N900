# MaemoBuild 交叉编译环境（已验证可用）

## 架构
WSL2 Ubuntu 22.04（F:\WSL\MaemoBuild，rootfs 从 cloud-images 导入）
+ gcc-arm-linux-gnueabi 11.4（软浮点 armel）
+ Fremantle sysroot（research/sysroot：277 个 -dev 包 + glibc 2.5 运行库，
  2619 符号链接清单 research/sysroot_symlinks.txt）

## 关键配方（踩坑结晶）
1. libc.so 链接脚本用 `=/lib/libc.so.6` 强制 sysroot 映射（`=` 前缀语法），
   脚本文件放 /usr/arm-linux-gnueabi/lib/libc.so（顶替 Ubuntu 交叉包版本，
   原版备份 .orig）
2. **禁用 crt 文件**：Fremantle 老 crt*.o 与现代 binutils 属性合并触发
   BFD 断言。解法 = -nostartfiles + 自写 _start（hello_ns.c 的 6 行 asm，
   bl main / bl exit），完全绕开
3. 编译命令：arm-linux-gnueabi-gcc --sysroot=/opt/sysroot
   -Wl,--sysroot=/opt/sysroot -nostartfiles ...
4. 产物特征（已真机验证）：GLIBC_2.4 唯一版本、interp=/lib/ld-linux.so.3、
   NEEDED libc.so.6

## 使用
wsl -d MaemoBuild -- bash /mnt/e/working/zcode-projects/Maemo_N900/crossenv/build5.sh
（改源文件名即可；sysroot_symlinks.txt 重建脚本 = research/build_links.sh）
