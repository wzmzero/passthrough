#pragma once
#include <string>
#include <vector>

// 用户对象
struct User {
    int id = 0;
    std::string name;
    std::string email;
    int age = 0;
    bool isAdmin = false;
};

// 文章对象
struct Post {
    int id = 0;
    std::string title;
    std::string content;
    int userId = 0;  // 外键
    int views = 0;
    double rating = 0.0;
};

// 评论对象
struct Comment {
    int id = 0;
    std::string text;
    int postId = 0;  // 外键
    int userId = 0;  // 外键
    std::string createdAt; // 时间戳
};

// 标签对象
struct Tag {
    int id = 0;
    std::string name;
};

// 文章标签关联对象
struct PostTag {
    int postId = 0;
    int tagId = 0;
};