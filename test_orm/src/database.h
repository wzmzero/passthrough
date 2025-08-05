#pragma once

#include "sqlite_orm/sqlite_orm.h"
#include "models.h"
#include <vector>
#include <ctime>

namespace sql = sqlite_orm;

// 创建数据库存储对象
inline auto initStorage(const std::string& path) {
    using namespace sqlite_orm;
    
    // 定义存储结构
    auto storage = make_storage(path,
        // 用户表
        make_table("users",
            make_column("id", &User::id, primary_key().autoincrement()),
            make_column("name", &User::name),
            make_column("email", &User::email, unique()),
            make_column("age", &User::age),
            make_column("is_admin", &User::isAdmin, default_value(false))
        ),
        
        // 文章表
        make_table("posts",
            make_column("id", &Post::id, primary_key().autoincrement()),
            make_column("title", &Post::title, collate_nocase()),
            make_column("content", &Post::content),
            make_column("user_id", &Post::userId),
            make_column("views", &Post::views, default_value(0)),
            make_column("rating", &Post::rating),
            foreign_key(&Post::userId).references(&User::id).on_delete.cascade() // 级联删除
        ),
        
        // 评论表
        make_table("comments",
            make_column("id", &Comment::id, primary_key().autoincrement()),
            make_column("text", &Comment::text),
            make_column("post_id", &Comment::postId),
            make_column("user_id", &Comment::userId),
            make_column("created_at", &Comment::createdAt, default_value("CURRENT_TIMESTAMP")),
            foreign_key(&Comment::postId).references(&Post::id).on_delete.cascade(),
            foreign_key(&Comment::userId).references(&User::id).on_delete.set_null()
        ),
        
        // 标签表
        make_table("tags",
            make_column("id", &Tag::id, primary_key().autoincrement()),
            make_column("name", &Tag::name, collate_nocase(), unique())
        ),
        
        // 文章标签关联表
        make_table("post_tags",
            make_column("post_id", &PostTag::postId),
            make_column("tag_id", &PostTag::tagId),
            primary_key(&PostTag::postId, &PostTag::tagId),
            foreign_key(&PostTag::postId).references(&Post::id).on_delete.cascade(),
            foreign_key(&PostTag::tagId).references(&Tag::id).on_delete.cascade()
        )
    );
    
    // 同步表结构
    storage.sync_schema( );
 
 
    return storage;
}

// 类型别名
using Storage = decltype(initStorage(""));