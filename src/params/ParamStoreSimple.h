#pragma once
#include "params/Param.h"
#include <mutex>
#include <unordered_map>
#include <memory>


// ===== Store (RT-friendly, без атомиков) =====
class ParameterStoreSimple : public IParameterStore {
public:
    // ---- НЕ RT: наполнение до старта аудио ----
    void add(std::unique_ptr<IParam> p) override {
        std::lock_guard<std::mutex> lk(mu_);
        buildMap_[p->meta().id] = std::move(p);
    }
    void addListener(Listener l) override {
        std::lock_guard<std::mutex> lk(mu_);
        buildListeners_.push_back(std::move(l));
    }
    void commitParams() override{                  // вызвать ОДИН раз до старта аудио
        std::lock_guard<std::mutex> lk(mu_);
        mapSnap_ = std::make_shared<Map>(std::move(buildMap_));
        buildMap_.clear();
    }
    void commitListeners() override {               // вызвать ОДИН раз до старта аудио
        std::lock_guard<std::mutex> lk(mu_);
        lsSnap_ = std::make_shared<std::vector<Listener>>(std::move(buildListeners_));
        buildListeners_.clear();
    }

    // ---- RT: только чтение снапшотов, никаких локов/аллок ----
    IParam* find(const std::string& id) override {
        // по договору: mapSnap_ уже установлен и не меняется
        if (!mapSnap_) return nullptr;
        auto it = mapSnap_->find(id);
        return (it == mapSnap_->end()) ? nullptr : it->second.get();
    }

    // Установка значения + нотификация слушателей
    bool set(const std::string& id, float value) noexcept {
        if (!mapSnap_) return false;
        auto it = mapSnap_->find(id);
        if (it == mapSnap_->end()) return false;

        it->second->setFloat(value);   // меняем ТОЛЬКО в аудио-треде

        if (lsSnap_) {                 // опциональная нотификация (дёшево)
            for (auto& cb : *lsSnap_) cb(id, value);
        }

        return true;
    }

    void notify(const std::string& id, float value) override {
        if (!lsSnap_) return;
        for (auto& cb : *lsSnap_) cb(id, value);
    }

    void dumpMap() override {
        for (const auto& pair : *mapSnap_) {
            std::cout << pair.first << " : " << pair.second->getFloat() << "\n";
        }
    }

private:
    using Map = std::unordered_map<std::string, std::unique_ptr<IParam>>;

    // build-структуры (меняются только в main до commit’ов)
    std::mutex              mu_;
    Map                     buildMap_;
    std::vector<Listener>   buildListeners_;

    // неизменяемые снапшоты после commit’ов (только читаются в RT)
    std::shared_ptr<Map>                     mapSnap_;
    std::shared_ptr<std::vector<Listener>>   lsSnap_;
};
