# Xmake

## 生成 compile_commands.json

```bash
xmake project -k compile_commands
```

## 编译

```bash
xmake build <target>
```

## 切换模式

```bash
xmake f -m debug (or release)
```

## 编译指定 target

```bash
xmake build <target>
```

## 编译全部

```bash
xmake build -a (or --all)
```

## 运行指定 target

```bash
xmake run <target>
```

## 运行指定工作目录

```bash
xmake run -w /path/to
```

## 指定 qt 目录：

```bash
xmake f --qt=~/Qt/6.8.3
```

## 指定 Windows mingw 以及 qt

```bash
xmake f -p mingw -a x86_64 --qt=~/Qt/6.8.3
```

~~注意：因为交叉编译 Qt 需要 mingw 的 Qt 工具链，如果没有配置的话，也会导致无法编译，但已经大致可以在 linux 上编辑 windows 代码了~~

更新：从 windows 上下载 mingw_64 target 的 Qt，加入本地 Qt 目录之后，可使用以下命令来使用正确的 Qt

```bash
xmake f -p mingw -a x86_64 --qt=~/Qt/6.8.3/mingw_64 --qt_host=~/Qt/6.8.3/gcc_64
```
