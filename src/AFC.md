# 关于自动频率校正(AFC)的说明
由于CIR发射信号的频率的不确定性，SX1276提供的AFC功能完全无法使用，本项目使用的是基于SX1276提供的FEI（Frequency Error Indicator，频率错误指示）数值进行自定义的自动频率校正操作。

根据频谱观察可知，CIR发送的信号大体可分为载波（Carrier）、前导（Preamble）、数据（Data）三个阶段，
由于SX1276提供的FEI仅在载波和前导阶段有效（根据数据手册），故目前仅在这两个阶段执行自动校频操作。

## 1. 载波阶段

### 实现方式

该阶段为空载波，即821.2375MHz（频偏取决于发射机）
的正弦波。根据SX1276数据手册中的框图可知，FSK的解调采用了正交解调后进行过零检测（Zero-Crossing）方案，
而Direct RX模式中DIO的输出实则为过零检测器的输出，故信号为空载波时，正交解调后应为恒定电平（DC），过零检测表现为连续的低电平`0`或高电平`1`。因此仅需要在
DIO上检测这种特征即可实现对载波的探测，具体代码实现如下：

在`RadioLib/src/Protocols/PhysicalLayer/PhysicalLayer.cpp`的函数`updateDirectBuffer()`中，有如下的代码用于检测前导码：

```c++
// check preamble
if (!this->gotPreamble && !this->gotSync) {
    this->preambleBuffer <<=1;
    this->preambleBuffer |= bit;

    if (this->preambleBuffer == 0xAAAAAAAAAAAAAAAA || this->preambleBuffer == 0x5555555555555555) {
        this->gotPreamble = true;
    }
}
```

仿照该代码和之前提到的原理，即可加入如下代码用于检测载波：

```c++
// check carrier
if (!this->gotCarrier && !this->gotPreamble && !this->gotSync) {
    this->carrierBuffer <<=1;
    this->carrierBuffer |= bit;

    if (this->carrierBuffer == 0x0000000000000000 || this->carrierBuffer == 0xFFFFFFFFFFFFFFFF) {
        this->gotCarrier = true;
    }
}
```

之后在`PagerMod.cpp`中加入函数`gotCarrierState()`用来传出`gotCarrier`变量的值，判断是否检测到了载波。该函数的代码如下：

```cpp
bool PagerClient::gotCarrierState() {
    if (phyLayer->gotCarrier) {
        phyLayer->gotCarrier = false;
        phyLayer->carrierBuffer = 0x9877FA3CA50B1DBD; // just a random number,
        // if initialize to 0 it will consider as a carrier.
        return true;
    } else
        return false;
}
```

为了防止载波检测锁定在触发状态，若载波检测为真每调用该函数后就会清空状态，并重新初始化载波检测缓存用于进行下一次检测。

检测到载波后，便可以对载波频率进行追踪。在`main.cpp`中定义了`handleCarrier()`函数用于执行载波追踪，其代码如下：

```cpp
void handleCarrier() {
    if (pager.gotCarrierState() && !pager.gotPreambleState() && !pager.gotSyncState() && freq_correction &&
        prb_timer == 0) { // 仅在未检测到前导码、同步码、频率校正开启的情况下执行
        if (car_count == 0)
            car_timer = millis64(); // 记录首个载波触发时间
        ++car_count; // 记录载波检测器触发次数
        if (car_count < 64) { // 超过64次仍为检测到前导或同步，判断为超时。
            float fei = radio.getFrequencyError();
            // Serial.printf("[D] Carrier FEI %.2f Hz, count %d\n",fei,car_count);
            if (abs(fei) > 1000.0 && car_count != 1 &&
                abs(fei - car_fer_last) < 500) { // 去除离群值
                // Perform frequency correction
                auto target_freq = (float) (actual_frequency + fei * 1e-6);
                int state = radio.setFrequency(target_freq);
                if (state != RADIOLIB_ERR_NONE) {
                    Serial.printf("[D][C] Freq Alter failed %d, target freq %f\n", state, target_freq);
                    sd1.append("[D][C] Freq Alter failed %d, target freq %f\n", state, target_freq);
                } else {
                    actual_frequency = target_freq;
                    Serial.printf("[D][C] Freq Altered %f MHz, FEI %.2f Hz, PPM %.2f\n", actual_frequency, fei,
                                  getBias(actual_frequency));
                }
            }
            car_fer_last = fei; // 记录上次测量频偏，用于进行离群值判断
        }
    }
}
```

由前文可知，每个载波检测通过检测到64位连续的0或1触发，若触发64次（也就是4096位连续的0或1）仍未进入前导或同步阶段，即认为该信号接收失败，不再进行频率校正。

由于CIR发射频率的不确定性，载波频率浮动造成的测量频偏值大幅度变化需要予以剔除。目前采用的方式为FEI绝对值大于1000且于上次FEI值的差值小于500的情况下才执行频率校正。

### 优势

早先频率校正只通过前导检阶段测来实现，受限于RxBW，超过该范围的信号将无法被检测或追踪。同样受限于需要准确检测到至少64位连续的前导码才能执行频率校正，有较大频率偏差的信号将无法被追踪和解码。

通过实测发现频偏较大的情况下，在信号进入前导码阶段后，虽然DIO不能准确输出前导码，但能够输出连续的高或低电平，与检测到载波的情况类似，并且是连续的高电平还是低电平与频偏的方向有关（原理尚不明确）。此时读取FEI的值虽然不够准确（因为没有接收到带宽完整的数据，根据数据手册FEI准确的条件为在载波或前导阶段接收到带宽完整`full-bandwidth`的信号），但其大体方向仍然正确（可理解为带一定超调量或欠调量的反馈值），故作为反馈值进行调节仍可让频率趋于收敛。故载波阶段执行频率校正不仅可实现载波追踪，也可实现频偏较大情况下的前导阶段追踪。

采用该方案后显著提高了频率追踪的带宽范围，根据实测可追踪范围达到数百kHz，远超12.5kHz的RxBW设定，应可以满足绝大多数情况下的频偏追踪需求。

### 问题
该方案引入了的问题有： 
- **抗干扰能力降低：** 由于载波检测为连续的低电平或高电平，并且范围较大，频率附近若存在正弦干扰信号便会导致接收器连续试图锁定在干扰信号上，并由于未检测到前导码和同步码不断超时，使接收器失去接收能力。具体表现为在串口上不断输出超时信息。
- **无法回正：** 根据实测，若信号较强且实际上频偏较大的信号被检测到并成功解码，接收器频率会根据接收到的信号被设置到一个远离中心频率的位置，并且其它较弱的信号将难被检测到以将接收器频率带回中心频率，使接收器失去接收能力。该情况较为罕见，偶尔发生，推测为CIR频偏较大或高铁多普勒偏移导致。

针对以上问题目前我仅有的解决思路为采用调整带宽而非载波检测的方法，即在接收前采用较大的RxBW，检测到前导码并基本完成追踪/频率收敛后切换到较小的RxBW完成信息的解调。但在之前的测试中改变RxBW会改变接收器的工作模式，导致接收器退出Direct RX模式。目前暂未找到解决方案，相关测试代码目前仍以注释的形式留在源码中，可以进行参考。
## 2. 前导阶段

前导阶段频率校正由`main.cpp`中`handlePreamble()`函数实现，代码如下（未使用的代码已去除）：

```c++
void handlePreamble() {
    if (pager.gotPreambleState() && !pager.gotSyncState() && freq_correction) {
        if (prb_count == 0)
            prb_timer = millis64();
        ++prb_count;
        if (prb_count < 32) {
            // todo: Implement automatic bandwidth adjustment.
            fers[prb_count - 1] = radio.getFrequencyError();
            if (abs(fers[prb_count - 1]) > 1000.0 && prb_count != 1 &&
                abs(fers[prb_count - 1] - fers[prb_count - 2]) < 500) {
                // Perform frequency correction
                auto target_freq = (float) (actual_frequency + fers[prb_count - 1] * 1e-6);
                int state = radio.setFrequency(target_freq);
                if (state != RADIOLIB_ERR_NONE) {
                    Serial.printf("[D][P] Freq Alter failed %d, target freq %f\n", state, target_freq);
                    sd1.append("[D][P] Freq Alter failed %d, target freq %f\n", state, target_freq);
                } else {
                    actual_frequency = target_freq;
                    Serial.printf("[D][P] Freq Altered %f MHz, FEI %.2f Hz, PPM %.2f\n", actual_frequency,
                                  fers[prb_count - 1], getBias(actual_frequency));
                }
            }
        }
    }
}
```
与载波阶段大体类似，同样限制了频率校正的执行条件以尽可能防止接收器被CIR的频率漂移“误导”。

## 3. 主循环调用
以上函数都具有超时判断，在主循环中对超时的处理如下：
```c++
void loop() {
    ...
    // Handle carrier timout.
    if (car_timer != 0 && millis64() - car_timer > 700 && prb_timer == 0 && rxInfo.timer == 0) {
        car_count = 0;
        revertFrequency();
        car_fer_last = 0;
        car_timer = 0;
        Serial.println("[D] CARRIER TIMEOUT.");
    }

    // Handle preamble timeout.
    if (prb_timer != 0 && millis64() - prb_timer > 600 && rxInfo.timer == 0) {
        prb_count = 0;
        revertFrequency();
        for (auto &i: fers) {
            i = 0;
        }
        prb_timer = 0;
        Serial.println("[D] PREAMBLE TIMEOUT.");
    }
    ...
}
```
总体上功能类似，均为初始化变量，并返回之前的频率。

以上函数在主循环中的调用如下：
```c++
void loop() {
    ...
    handleCarrier();
    handlePreamble();

    handleSync();
    ...
}
```
同步码过程中因FEI不准确，不再进行频率校正，只进行RSSI的测量。
