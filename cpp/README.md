# Xmake

生成 compile_commands.json

```bash
xmake project -k compile_commands
```

编译

```bash
xmake build <target>
```

切换模式

```bash
xmake f -m debug (or release)
```

编译指定 target

```bash
xmake build <target>
```

编译全部

```bash
xmake build -a (or --all)
```

运行指定 target

```bash
xmake run <target>
```

运行指定工作目录

```bash
xmake run -w /path/to
```
