# j2me/ — J2ME 开发工作区

**先读 [HANDOFF.md](./HANDOFF.md)** —— 交接文档（资料清单 + 技术选型 + 前期调研结论 + 下一步）。

## 快速开始

```bash
# 资料都在本地（不入 git，约 878MB）
j2me/tools/wtk252_win.exe              # Sun 官方 WTK 2.5.2
j2me/emulators/KEmulator-nnmod/        # ★首选模拟器
j2me/references/awesome-j2me/readme.md # J2ME 全景清单
```

## 环境提醒

- 本机 Java 26，**KEmulator nnmod 要求 Java 8** —— 开工第一件事是解决这个
- 开发循环可以在 PC 完全闭环（编译→模拟器→截图验证），不用开真机
- 部署到 E5：`.jar` 拷进去即可，**无需签名**
