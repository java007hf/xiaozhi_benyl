# ESP32 set-target 修复记录

日期：2026-05-19

## 背景

执行 `idf.py set-target esp32` 时，最初失败在组件解析阶段：

```text
Failed to resolve component 'json' required by component
'espressif2022__esp_emote_expression': unknown name.
```

项目使用 ESP-IDF v6.0.1，`espressif2022/esp_emote_expression 1.0.1` 的
`CMakeLists.txt` 仍声明依赖旧组件名 `json`，而当前环境实际可用的是
`espressif__cjson`。

## 修改内容

1. 新增本地兼容组件 `components/json/CMakeLists.txt`，将旧组件名 `json`
   转接到当前 managed component `espressif__cjson`：

   ```cmake
   idf_component_register(
       REQUIRES espressif__cjson
   )
   ```

2. 调整 `main/idf_component.yml` 中的 JPEG 组件版本约束：

   ```yaml
   espressif/esp_new_jpeg: 1.*
   ```

   原因是 `espressif2022/esp_emote_gfx 3.0.5` 依赖
   `espressif/esp_new_jpeg (1.*)`，与项目原来的 `^0.6.1` 冲突。

## 环境处理

`xtensa-esp32-elf-*` 目标包装器会在当前 Windows 环境中 panic：

```text
thread 'main' panicked at main.rs:54:9:
assertion `left != right` failed: Failed to get path name. Error code: 5
```

处理步骤：

1. 删除并重新下载 `xtensa-esp-elf@esp-15.2.0_20251204` 工具链。
2. 确认通用 `xtensa-esp-elf-*` 可执行文件正常。
3. 备份损坏的 `xtensa-esp32-elf-*` 包装器，并用对应的
   `xtensa-esp-elf-*` 可执行文件替换。

备份目录：

```text
C:\Users\Administrator\.espressif\tools\xtensa-esp-elf\esp-15.2.0_20251204\xtensa-esp-elf\bin\_broken_xtensa_esp32_wrappers_backup
```

## 验证结果

重新执行：

```powershell
idf.py set-target esp32
```

结果成功：

```text
Configuring done
Generating done
Build files have been written to: C:/workspace/xiaozhi-esp32/build
```

## 注意事项

`components/` 目录被 `.gitignore` 忽略，因此兼容组件需要用 `git add -f`
加入版本控制：

```powershell
git add -f components/json/CMakeLists.txt
```
