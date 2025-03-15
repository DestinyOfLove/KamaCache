#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <functional>
#include <algorithm>
#include <map>

// 检查是否支持 C++14 标准的 make_unique
#if __cplusplus >= 201402L || (defined(_MSC_VER) && _MSC_VER >= 1900)
    // 如果支持 C++14，使用标准的 make_unique
    using std::make_unique;
#else
    // 如果不支持 C++14，自己实现 make_unique
    template<typename T, typename... Args>
    std::unique_ptr<T> make_unique(Args&&... args) {
        return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
    }
#endif

#include "KArcCache/KArcCache.h"
#include "KICachePolicy.h"
#include "KLfuCache.h"
#include "KLruCache.h"

// 定义缓存算法类型枚举
enum class CacheType {
    LRU,
    LFU,
    ARC
};

// 基础测试类
class CacheTest {
protected:
    // 测试配置
    struct TestConfig {
        int capacity;               // 缓存容量
        int operations;             // 操作次数
        bool showProgressBar;       // 是否显示进度条
        int progressInterval;       // 进度更新间隔
        
        TestConfig() : capacity(50), operations(100000), showProgressBar(true), progressInterval(10000) {}
    };
    
    TestConfig config;
    std::string testName;
    
    // 缓存实例
    std::unique_ptr<KamaCache::KLruCache<int, std::string>> lruCache;
    std::unique_ptr<KamaCache::KLfuCache<int, std::string>> lfuCache;
    std::unique_ptr<KamaCache::KArcCache<int, std::string>> arcCache;
    
    // 测试统计
    std::map<CacheType, int> hits;
    std::map<CacheType, int> operations;
    
    // 随机数生成器
    std::mt19937 gen;
    
    // 显示进度条
    void showProgress(int current, int total) const {
        if (!config.showProgressBar) return;
        if (current % config.progressInterval != 0 && current != total) return;
        
        int barWidth = 50;
        float progress = static_cast<float>(current) / total;
        int pos = static_cast<int>(barWidth * progress);
        
        std::cout << "[";
        for (int i = 0; i < barWidth; ++i) {
            if (i < pos) std::cout << "=";
            else if (i == pos) std::cout << ">";
            else std::cout << " ";
        }
        std::cout << "] " << int(progress * 100.0) << "% (" << current << "/" << total << ")\r";
        std::cout.flush();
        
        if (current == total) std::cout << std::endl;
    }
    
    // 获取缓存对象
    KamaCache::KICachePolicy<int, std::string>* getCache(CacheType type) {
        switch (type) {
            case CacheType::LRU: return lruCache.get();
            case CacheType::LFU: return lfuCache.get();
            case CacheType::ARC: return arcCache.get();
        }
        return nullptr;
    }
    
    // 初始化缓存实例
    void initCaches() {
        lruCache = make_unique<KamaCache::KLruCache<int, std::string>>(config.capacity);
        lfuCache = make_unique<KamaCache::KLfuCache<int, std::string>>(config.capacity);
        arcCache = make_unique<KamaCache::KArcCache<int, std::string>>(config.capacity);
        
        // 重置统计数据
        hits[CacheType::LRU] = 0;
        hits[CacheType::LFU] = 0;
        hits[CacheType::ARC] = 0;
        operations[CacheType::LRU] = 0;
        operations[CacheType::LFU] = 0;
        operations[CacheType::ARC] = 0;
    }
    
    // 执行测试的核心方法（由子类实现）
    virtual void runTestForCacheType(CacheType type) = 0;
    
public:
    CacheTest(const std::string& name) : testName(name), gen(std::random_device{}()) {
        initCaches();
    }
    
    virtual ~CacheTest() = default;
    
    // 设置测试配置
    void setCapacity(int capacity) {
        config.capacity = capacity;
        initCaches();  // 重新初始化缓存以应用新容量
    }
    
    void setOperations(int operations) {
        config.operations = operations;
    }

    void setShowProgressBar(bool show) {
        config.showProgressBar = show;
    }
    
    void setProgressInterval(int interval) {
        config.progressInterval = interval;
    }
    
    // 运行测试
    void runTest() {
        std::cout << "\n===== 开始测试：" << testName << " =====" << std::endl;
        std::cout << "参数配置：" << std::endl;
        std::cout << "- 缓存容量: " << config.capacity << std::endl;
        std::cout << "- 操作次数: " << config.operations << std::endl;
        
        // 附加参数由子类输出
        printTestParameters();
        
        // 记录测试开始时间
        auto startTime = std::chrono::high_resolution_clock::now();
        
        // 对每种缓存类型运行测试
        std::cout << "\n测试 LRU 缓存..." << std::endl;
        runTestForCacheType(CacheType::LRU);
        
        std::cout << "\n测试 LFU 缓存..." << std::endl;
        runTestForCacheType(CacheType::LFU);
        
        std::cout << "\n测试 ARC 缓存..." << std::endl;
        runTestForCacheType(CacheType::ARC);
        
        // 计算测试耗时
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        // 输出测试结果
        printResults(duration);
    }
    
    // 输出测试特定参数（由子类实现）
    virtual void printTestParameters() const = 0;
    
    // 输出测试结果
    void printResults(const std::chrono::milliseconds& duration) const {
        std::cout << "\n----- 测试结果：" << testName << " -----" << std::endl;
        std::cout << "测试耗时: " << duration.count() / 1000.0 << " 秒" << std::endl;
        std::cout << "缓存容量: " << config.capacity << std::endl;
        
        // 计算命中率
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "LRU - 操作数: " << operations.at(CacheType::LRU) 
                  << ", 命中数: " << hits.at(CacheType::LRU)
                  << ", 命中率: " << (100.0 * hits.at(CacheType::LRU) / operations.at(CacheType::LRU)) << "%"
                  << std::endl;
        
        std::cout << "LFU - 操作数: " << operations.at(CacheType::LFU)
                  << ", 命中数: " << hits.at(CacheType::LFU)
                  << ", 命中率: " << (100.0 * hits.at(CacheType::LFU) / operations.at(CacheType::LFU)) << "%"
                  << std::endl;
        
        std::cout << "ARC - 操作数: " << operations.at(CacheType::ARC)
                  << ", 命中数: " << hits.at(CacheType::ARC)
                  << ", 命中率: " << (100.0 * hits.at(CacheType::ARC) / operations.at(CacheType::ARC)) << "%"
                  << std::endl;
    }
};

// 热点数据访问测试类
class HotDataAccessTest : public CacheTest {
private:
    int hotKeys;    // 热点数据数量
    int coldKeys;   // 冷数据数量
    int hotRatio;   // 热点数据访问比例 (百分比)
    
protected:
    void runTestForCacheType(CacheType type) override {
        auto cache = getCache(type);
        if (!cache) return;
        
        // 重置统计
        hits[type] = 0;
        operations[type] = 0;
        
        std::cout << "\n1. 填充缓存数据阶段" << std::endl;
        // 先进行一系列put操作
        for (int op = 0; op < config.operations; ++op) {
            int key;
            if (op % 100 < hotRatio) {  // hotRatio%热点数据
                key = gen() % hotKeys;
            } else {  // (100-hotRatio)%冷数据
                key = hotKeys + (gen() % coldKeys);
            }
            std::string value = "value" + std::to_string(key);
            cache->put(key, value);
            
            showProgress(op + 1, config.operations);
        }
        
        std::cout << "\n2. 测试缓存访问阶段" << std::endl;
        // 然后进行随机get操作
        for (int get_op = 0; get_op < config.operations; ++get_op) {
            int key;
            if (get_op % 100 < hotRatio) {  // hotRatio%概率访问热点
                key = gen() % hotKeys;
            } else {  // (100-hotRatio)%概率访问冷数据
                key = hotKeys + (gen() % coldKeys);
            }
            
            std::string result;
            operations[type]++;
            if (cache->get(key, result)) {
                hits[type]++;
            }
            
            showProgress(get_op + 1, config.operations);
        }
    }
    
    void printTestParameters() const override {
        std::cout << "- 热点数据数量: " << hotKeys << std::endl;
        std::cout << "- 冷数据数量: " << coldKeys << std::endl;
        std::cout << "- 热点数据访问比例: " << hotRatio << "%" << std::endl;
    }
    
public:
    HotDataAccessTest()
        : CacheTest("热点数据访问测试"),
          hotKeys(20),
          coldKeys(5000),
          hotRatio(70) {}
    
    void setHotKeys(int count) {
        hotKeys = count;
    }
    
    void setColdKeys(int count) {
        coldKeys = count;
    }
    
    void setHotRatio(int ratio) {
        if (ratio < 0) ratio = 0;
        if (ratio > 100) ratio = 100;
        hotRatio = ratio;
    }
};

// 循环扫描测试类
class LoopPatternTest : public CacheTest {
private:
    int loopSize;           // 循环大小
    int sequentialRatio;    // 顺序扫描比例 (百分比)
    int randomRatio;        // 随机访问比例 (百分比)
    // 注意: 超出范围访问比例 = 100 - sequentialRatio - randomRatio
    
protected:
    void runTestForCacheType(CacheType type) override {
        auto cache = getCache(type);
        if (!cache) return;
        
        // 重置统计
        hits[type] = 0;
        operations[type] = 0;
        
        std::cout << "\n1. 填充缓存数据阶段" << std::endl;
        // 先填充数据
        for (int key = 0; key < loopSize; ++key) {
            std::string value = "loop" + std::to_string(key);
            cache->put(key, value);
            
            if (key % 100 == 0 || key == loopSize - 1) {
                showProgress(key + 1, loopSize);
            }
        }
        
        std::cout << "\n2. 测试缓存访问阶段" << std::endl;
        // 然后进行访问测试
        int current_pos = 0;
        for (int op = 0; op < config.operations; ++op) {
            int key;
            int r = op % 100;
            
            if (r < sequentialRatio) {  // 顺序扫描
                key = current_pos;
                current_pos = (current_pos + 1) % loopSize;
            } else if (r < sequentialRatio + randomRatio) {  // 随机跳跃
                key = gen() % loopSize;
            } else {  // 访问范围外数据
                key = loopSize + (gen() % loopSize);
            }
            
            std::string result;
            operations[type]++;
            if (cache->get(key, result)) {
                hits[type]++;
            }
            
            showProgress(op + 1, config.operations);
        }
    }
    
    void printTestParameters() const override {
        std::cout << "- 循环数据大小: " << loopSize << std::endl;
        std::cout << "- 顺序扫描比例: " << sequentialRatio << "%" << std::endl;
        std::cout << "- 随机访问比例: " << randomRatio << "%" << std::endl;
        std::cout << "- 超出范围访问比例: " << (100 - sequentialRatio - randomRatio) << "%" << std::endl;
    }
    
public:
    LoopPatternTest()
        : CacheTest("循环扫描测试"),
          loopSize(500),
          sequentialRatio(60),
          randomRatio(30) {}
    
    void setLoopSize(int size) {
        loopSize = size;
    }
    
    void setSequentialRatio(int ratio) {
        if (ratio < 0) ratio = 0;
        if (ratio > 100) ratio = 100;
        sequentialRatio = ratio;
        // 确保总比例不超过100%
        if (sequentialRatio + randomRatio > 100) {
            randomRatio = 100 - sequentialRatio;
        }
    }
    
    void setRandomRatio(int ratio) {
        if (ratio < 0) ratio = 0;
        if (ratio > 100) ratio = 100;
        randomRatio = ratio;
        // 确保总比例不超过100%
        if (sequentialRatio + randomRatio > 100) {
            sequentialRatio = 100 - randomRatio;
        }
    }
};

// 工作负载变化测试类
class WorkloadShiftTest : public CacheTest {
private:
    int initialDataSize;    // 初始数据大小
    int phases;             // 阶段数量
    int putProbability;     // 执行put操作的概率 (百分比)
    
protected:
    void runTestForCacheType(CacheType type) override {
        auto cache = getCache(type);
        if (!cache) return;
        
        // 重置统计
        hits[type] = 0;
        operations[type] = 0;
        
        std::cout << "\n1. 填充初始数据阶段" << std::endl;
        // 先填充一些初始数据
        for (int key = 0; key < initialDataSize; ++key) {
            std::string value = "init" + std::to_string(key);
            cache->put(key, value);
            
            if (key % 100 == 0 || key == initialDataSize - 1) {
                showProgress(key + 1, initialDataSize);
            }
        }
        
        const int phaseLength = config.operations / phases;
        
        std::cout << "\n2. 多阶段测试阶段" << std::endl;
        // 然后进行多阶段测试
        for (int op = 0; op < config.operations; ++op) {
            int key;
            int phase = op / phaseLength;
            
            // 输出阶段变化信息
            if (op % phaseLength == 0) {
                std::cout << "\n   开始阶段 " << (phase + 1) << "/" << phases << std::endl;
            }
            
            // 根据不同阶段选择不同的访问模式
            switch (phase % 5) {
                case 0:  // 热点访问
                    key = gen() % 5;
                    break;
                case 1:  // 大范围随机
                    key = gen() % initialDataSize;
                    break;
                case 2:  // 顺序扫描
                    key = (op - phase * phaseLength) % 100;
                    break;
                case 3:  // 局部性随机
                    {
                        int locality = (op / 1000) % 10;
                        key = locality * 20 + (gen() % 20);
                    }
                    break;
                case 4:  // 混合访问
                default:
                    {
                        int r = gen() % 100;
                        if (r < 30) {
                            key = gen() % 5;
                        } else if (r < 60) {
                            key = 5 + (gen() % 95);
                        } else {
                            key = 100 + (gen() % (initialDataSize - 100));
                        }
                    }
                    break;
            }
            
            std::string result;
            operations[type]++;
            if (cache->get(key, result)) {
                hits[type]++;
            }
            
            // 随机进行put操作，更新缓存内容
            if (gen() % 100 < putProbability) {
                std::string value = "new" + std::to_string(key);
                cache->put(key, value);
            }
            
            showProgress(op + 1, config.operations);
        }
    }
    
    void printTestParameters() const override {
        std::cout << "- 初始数据大小: " << initialDataSize << std::endl;
        std::cout << "- 阶段数量: " << phases << std::endl;
        std::cout << "- 每阶段操作数: " << (config.operations / phases) << std::endl;
        std::cout << "- 写操作概率: " << putProbability << "%" << std::endl;
        
        // 描述各阶段工作负载特点
        std::cout << "- 阶段特点: " << std::endl;
        std::cout << "  * 阶段1: 热点访问 (少量key高频访问)" << std::endl;
        std::cout << "  * 阶段2: 大范围随机访问" << std::endl;
        std::cout << "  * 阶段3: 顺序扫描访问" << std::endl;
        std::cout << "  * 阶段4: 局部性随机访问" << std::endl;
        std::cout << "  * 阶段5: 混合访问模式" << std::endl;
    }
    
public:
    WorkloadShiftTest()
        : CacheTest("工作负载剧烈变化测试"),
          initialDataSize(1000),
          phases(5),
          putProbability(30) {}
    
    void setInitialDataSize(int size) {
        initialDataSize = size;
    }
    
    void setPhases(int count) {
        phases = count;
    }
    
    void setPutProbability(int probability) {
        if (probability < 0) probability = 0;
        if (probability > 100) probability = 100;
        putProbability = probability;
    }
};

// 测试管理器，可以轻松运行和管理多个测试
class CacheTestManager {
private:
    std::vector<std::shared_ptr<CacheTest>> tests;
    
public:
    // 添加测试
    template<typename T>
    std::shared_ptr<T> addTest() {
        auto test = std::make_shared<T>();
        tests.push_back(test);
        return test;
    }
    
    // 运行所有测试
    void runAllTests() {
        std::cout << "\n===============================" << std::endl;
        std::cout << "开始运行 " << tests.size() << " 个缓存策略测试" << std::endl;
        std::cout << "===============================" << std::endl;
        
        for (auto& test : tests) {
            test->runTest();
        }
        
        std::cout << "\n===============================" << std::endl;
        std::cout << "所有测试已完成" << std::endl;
        std::cout << "===============================" << std::endl;
    }
    
    // 清除所有测试
    void clearTests() {
        tests.clear();
    }
};

int main() {
    // 创建测试管理器
    CacheTestManager testManager;
    
    // 配置热点数据访问测试
    auto hotTest = testManager.addTest<HotDataAccessTest>();
    hotTest->setCapacity(50);
    hotTest->setOperations(200000);  // 减少操作数以加快测试
    hotTest->setHotKeys(20);
    hotTest->setColdKeys(5000);
    hotTest->setHotRatio(70);  // 70%的访问是热点数据
    
    // 配置循环扫描测试
    auto loopTest = testManager.addTest<LoopPatternTest>();
    loopTest->setCapacity(50);
    loopTest->setOperations(100000);
    loopTest->setLoopSize(500);
    loopTest->setSequentialRatio(60);  // 60%顺序扫描
    loopTest->setRandomRatio(30);      // 30%随机访问
    
    // 配置工作负载变化测试
    auto workloadTest = testManager.addTest<WorkloadShiftTest>();
    workloadTest->setCapacity(4);
    workloadTest->setOperations(50000);
    workloadTest->setInitialDataSize(1000);
    workloadTest->setPhases(5);
    workloadTest->setPutProbability(30);  // 30%概率写入
    
    // 运行所有测试
    testManager.runAllTests();
    
    return 0;
}