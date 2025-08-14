#include <odb/database.hxx>
#include <odb/transaction.hxx>
#include <odb/session.hxx>
#include <odb/sqlite/database.hxx>
#include "models.h"
#include "models-odb.hxx"

using namespace odb::core;

int main() {
    // 创建数据库连接
    auto db = std::make_shared<odb::sqlite::database>(
        "telemetry.db", SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE
    );
    
    // 创建表
    {
        transaction t(db->begin());
        db->execute("DROP TABLE IF EXISTS \"TelemPoint\"");
        db->execute("DROP TABLE IF EXISTS \"MasterPoint\"");
        db->execute("DROP TABLE IF EXISTS \"SlavePoint\"");
        schema_catalog::create_schema(*db);
        t.commit();
    }
    
    // 创建并保存遥测点
    auto ycPoint = std::make_shared<TelemPoint>();
    ycPoint->name = "Voltage";
    ycPoint->register_address = "0x4001";
    ycPoint->data_type = TelemDataType::YC;
    ycPoint->set_value(220.5f);
    ycPoint->timestamp = std::time(nullptr);
    ycPoint->unit = "V";
    
    {
        transaction t(db->begin());
        db->persist(ycPoint);
        t.commit();
    }
    
    // 创建并保存主点
    auto masterPoint = std::make_shared<MasterPoint>();
    masterPoint->name = "Switch";
    masterPoint->register_address = "0x6001";
    masterPoint->data_type = TelemDataType::YK;
    masterPoint->set_value(true);
    masterPoint->timestamp = std::time(nullptr);
    masterPoint->rw_flag = 1;
    
    {
        transaction t(db->begin());
        db->persist(masterPoint);
        t.commit();
    }
    
    // 创建从点并关联到遥测点
    auto slavePoint = std::make_shared<SlavePoint>();
    slavePoint->base = ycPoint;
    
    {
        transaction t(db->begin());
        db->persist(slavePoint);
        t.commit();
    }
    
    // 查询所有遥测点
    {
        session s;
        transaction t(db->begin());
        odb::result<TelemPoint> result = db->query<TelemPoint>();
        for (auto& point : result) {
            std::cout << "Point: " << point.name 
                      << " (" << point.register_address << ")\n";
        }
        t.commit();
    }
    
    // 更新主点状态
    {
        session s;
        transaction t(db->begin());
        auto mp = db->load<MasterPoint>(masterPoint->id);
        mp->return_flag = 1;
        db->update(mp);
        t.commit();
    }
    
    // 查询从点及其关联的遥测点
    {
        session s;
        transaction t(db->begin());
        typedef odb::query<SlavePoint> Query;
        typedef odb::result<SlavePoint> Result;
        
        Result r = db->query<SlavePoint>();
        for (auto& sp : r) {
            std::cout << "Slave Point ID: " << sp.id << "\n";
            std::cout << "Base Point: " << sp.base->name 
                      << " Value: ";
                      
            if (sp.base->value_type == ValueType::Float) {
                std::cout << sp.base->get_value<float>();
            }
            std::cout << "\n";
        }
        t.commit();
    }
    
    return 0;
}