# 中继

## 基本流程

1. 前端调用中继扫描接口扫描 WiFi，获取 AP 列表
2. 用户选择指定的 AP，调用中继连接接口，控制中继程序执行连接 WiFi 操作

## 额外功能

* 自动切换开关
* 锁定 BSSID：SSID 相同的多个 BSS，连接指定的 BSS
* 锁定频段：2.4G，5G
* 使用指定的 MAC 地址进行连接：出厂，克隆，随机
* 使用静态 IP 地址
* 设置 TTL, HL, MTU 参数
* 优化连接具有 portal 功能的 AP

## 代码组织结构

### 主程序

源码位置：gl-sdk4-repeater-v2/files/gl-repeater.lua
安装路径：/usr/sbin/gl-repeater

### 库

源码位置：gl-sdk4-repeater-v2/files/lib
安装路径：/usr/local/lib/lua/5.4/gl/

### API

源码位置：gl-sdk4-repeater-v2/files/rpc/repeater.lua
安装路径：/usr/lib/oui-httpd/rpc/repeater

## 对不同 WiFi 驱动类型模块化设计

gl-sdk4-repeater-v2/files/lib/repeater-mtk.lua

gl-sdk4-repeater-v2/files/lib/repeater-nl80211.lua

编译时，根据目标平台，会安装对应的平台相关的库，安装路径：/usr/local/lib/lua/5.4/gl/repeater-vendor.lua

主程序加载 repeater-vendor.lua，调用其提供的方法，主程序不必关心具体平台的差异。

## 程序启动流程

1. 初始化 trace 功能
2. 初始化日志
3. 加载中继配置
4. 加载 WiFi 配置
5. 创建 repeater 对象
6. 注册 ubus 服务
7. 监听 ubus 事件：network.interface
8. 监听 ubsu 事件：ntp.valid
9. 启动中继切换

主循环

```lua
function methods:run()
    while true do
        self.running = false
        self:notify_status()
        self.cond_switch:wait()
        self.running = true
        self:__run()
    end
end
```

调用 switch 方法，设置定时器

```lua
function methods:switch(delay)
    delay = delay or 0

    log.info('switch in', delay, 'seconds...')

    self.switch_tmr:set(delay)
end
```

定时器回调函数

```lua
o.switch_tmr = time.timer(function()
    local ok
    repeat
        ok = o.cond_switch:signal()
        if not ok then
            time.sleep(0.2)
        end
    until ok
end)
```
