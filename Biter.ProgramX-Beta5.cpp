#include <iostream>
#include <fstream>
#include <windows.h>
#include <cstdlib>
#include <ctime>
#include <stdexcept>
#include <map>
#include <string>
#include <sstream>
#include <limits>
#include <shellapi.h>
#include <tchar.h>
#include <cstdio>
#include <iomanip>      
#include <conio.h>      
#include <vector>       
#include <algorithm>    
#include <wincrypt.h>   // SHA256

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")   // 原为 user32.dll，改为 .lib
#pragma comment(lib, "shell32.lib")  // 托盘图标需要

#define TIP_ID          0x1000
#define MAX_INPUT_BUFFER 8192
#define WM_TRAY_MSG     (WM_USER + 100)

class ErrorDefine {
public:
    static constexpr unsigned long long LOGIN_ERROR = 0x000000000000004F;
    static constexpr unsigned long long LOGIN_ERROR_NAME_CAN_NOT_FALL_TO_THE_PROGRAM = 0x000000000000002F;
    static constexpr unsigned long long REGISTER_ERROR = 0x000000000000006F;
};

const std::string YOU = "You>";

typedef long double number;   // 用于计算器

static void console_clear();
static std::string current_time_str();
static void title(std::string title);

// 生存游戏声明
class sv_game {
    int health, hunger, air, moisture;
    std::map<std::string, int> Bag = {
        {"wood", 0},
        {"arrow", 100},
        {"bow", 100},
        {"stick", 0},
        {"stone", 0},
        {"iron", 0},
        {"thread", 0},
        {"meat", 0},
        {"fish", 0},
        {"grass", 0},
        {"leather", 0}   // 新增，支持 kill 中的皮革
    };
public:
    void set(std::string name, int var);
    void find();
    void eat();
    void sleep();
    void drink();
    void build();
    void fill();
    void kill();
    void fishing();
    void start();
    void check();
};

void sv_game::set(std::string name, int var) {
    if (name == "he") {
        this->health = var;
    }
    else if (name == "hu") {
        this->hunger = var;
    }
    else if (name == "ai") {
        this->air = var;
    }
    else if (name == "mo") {
        this->moisture = var;
    }
    else {
        return;
    }
}

void sv_game::find() {
    int tall = 0;
    std::cout << "输入高度:";
    std::cin >> tall;
    if (tall > 50) {
        this->Bag["stone"] += rand() % 20;
        if ((rand() % 200 - tall) > 100) {
            this->Bag["iron"] += rand() % 3;
        }
    }
    else if (tall < 10 and tall > -5) {
        this->Bag["grass"] += rand() % 10;
        if ((rand() % 50 - tall) > 28) {
            this->Bag["wood"] += rand() % 10;
        }
    }
    else if (tall < -5) {
        this->Bag["stone"] += rand() % 20;
        if ((rand() % 50 - tall) > 20) {
            this->Bag["iron"] += rand() % 10;
        }
    }
    else if (tall < -50) {
        this->Bag["stone"] += rand() % 45;
        if ((rand() % 100 - tall) > 120) {
            this->Bag["iron"] += rand() % 10;
        }
    }
}

void sv_game::fill() {
    if (Bag["grass"] < 3) {
        std::cout << "草不够！需要 3 个草才能搓成一根线。" << std::endl;
        return;
    }
    Bag["grass"] -= 3;
    Bag["thread"] += 1;
    std::cout << "你用 3 个草搓成了一根结实的线，线 +1" << std::endl;
}

void sv_game::check() {
    std::cout << "========== 状态检查 ==========" << std::endl;
    std::cout << "生命值 (Health) : " << health << std::endl;
    std::cout << "饱食度 (Hunger) : " << hunger << std::endl;
    std::cout << "氧气值 (Air)    : " << air << std::endl;
    std::cout << "水分值 (Moisture): " << moisture << std::endl;
    std::cout << "----------- 背包物品 -----------" << std::endl;
    bool hasItems = false;
    for (const auto& item : Bag) {
        if (item.second > 0) {
            std::cout << item.first << " : " << item.second << std::endl;
            hasItems = true;
        }
    }
    if (!hasItems) {
        std::cout << "（背包为空）" << std::endl;
    }
    std::cout << "===============================" << std::endl;

    if (health <= 20) std::cout << "[警告] 生命值过低！" << std::endl;
    if (hunger <= 20) std::cout << "[警告] 饥饿值过低！" << std::endl;
    if (air <= 20)    std::cout << "[警告] 氧气不足！" << std::endl;
    if (moisture <= 20) std::cout << "[警告] 水分不足！" << std::endl;
}

void sv_game::eat() {
    std::string choice;
    std::cout << "要吃什么？(meat / fish) : ";
    std::cin >> choice;

    if (choice == "meat" && Bag["meat"] > 0) {
        Bag["meat"]--;
        hunger += 30;
        health += 5;
        std::cout << "你吃了一份肉，饱食度 +30，生命 +5" << std::endl;
    }
    else if (choice == "fish" && Bag["fish"] > 0) {
        Bag["fish"]--;
        hunger += 25;
        health += 3;
        std::cout << "你吃了一条鱼，饱食度 +25，生命 +3" << std::endl;
    }
    else {
        std::cout << "没有这种食物或库存不足！" << std::endl;
    }
    if (hunger > 100) hunger = 100;
    if (health > 100) health = 100;
}

void sv_game::sleep() {
    if (hunger < 10) {
        std::cout << "太饿了，睡不着！" << std::endl;
        return;
    }
    hunger -= 10;
    health += 20;
    moisture += 10;
    if (health > 100) health = 100;
    if (moisture > 100) moisture = 100;
    std::cout << "你睡了一觉，生命 +20，水分 +10，饱食度 -10" << std::endl;
}

void sv_game::drink() {
    moisture += 20;
    if (moisture > 100) moisture = 100;
    std::cout << "你喝了些水，水分 +20" << std::endl;
}

void sv_game::build() {
    std::string what;
    std::cout << "你想建造什么？(house / bridge / weapon) : ";
    std::cin >> what;

    if (what == "house") {
        if (Bag["wood"] >= 10 && Bag["stone"] >= 5) {
            Bag["wood"] -= 10;
            Bag["stone"] -= 5;
            std::cout << "你建造了一间木石房屋！" << std::endl;
        }
        else {
            std::cout << "材料不足！需要木材 x10，石头 x5" << std::endl;
        }
    }
    else if (what == "bridge") {
        if (Bag["wood"] >= 5 && Bag["iron"] >= 2) {
            Bag["wood"] -= 5;
            Bag["iron"] -= 2;
            std::cout << "你建造了一座铁木桥！" << std::endl;
        }
        else {
            std::cout << "材料不足！需要木材 x5，铁 x2" << std::endl;
        }
    }
    else if (what == "weapon") {
        if (Bag["stick"] >= 1 && Bag["stone"] >= 2) {
            Bag["stick"]--;
            Bag["stone"] -= 2;
            std::cout << "你制作了一把石斧！" << std::endl;
        }
        else {
            std::cout << "材料不足！需要木棍 x1，石头 x2" << std::endl;
        }
    }
    else {
        std::cout << "未知建筑类型" << std::endl;
    }
}

void sv_game::kill() {
    if (Bag["arrow"] <= 0) {
        std::cout << "没有箭了！" << std::endl;
        return;
    }
    Bag["arrow"]--;
    int loot = rand() % 100;
    if (loot < 40) {
        Bag["meat"] += 2;
        std::cout << "你杀死了一只鹿，获得 2 块肉" << std::endl;
    }
    else if (loot < 70) {
        Bag["meat"] += 1;
        Bag["leather"]++;
        std::cout << "你杀死了一只狼，获得 1 块肉和 1 张皮" << std::endl;
    }
    else {
        Bag["fish"] += 1;
        std::cout << "你杀死了一只水獭，获得 1 条鱼" << std::endl;
    }
}

void sv_game::fishing() {
    if (Bag["stick"] < 1 || Bag["thread"] < 1) {
        std::cout << "没有鱼竿（需要木棍和线）" << std::endl;
        return;
    }
    int catch_num = rand() % 3 + 1;
    Bag["fish"] += catch_num;
    std::cout << "你钓到了 " << catch_num << " 条鱼！" << std::endl;
}

void sv_game::start() {
    srand(static_cast<unsigned>(time(nullptr)));

    health = 100;
    hunger = 80;
    air = 100;
    moisture = 80;

    Bag["wood"] = 5;
    Bag["stone"] = 3;
    Bag["stick"] = 2;
    Bag["thread"] = 1;

    std::cout << "========== 生存游戏开始 ==========" << std::endl;
    std::cout << "输入 'help' 查看所有命令。\n" << std::endl;

    while (true) {
        this->check();

        if (this->health <= 0) {
            std::cout << "\n 你死了！生命值为 0。" << std::endl;
            break;
        }
        if (this->hunger <= 0) {
            std::cout << "\n 你饿死了！饱食度为 0。" << std::endl;
            break;
        }
        if (this->air <= 0) {
            std::cout << "\n 你窒息了！氧气为 0。" << std::endl;
            break;
        }
        if (this->moisture <= 0) {
            std::cout << "\n 你渴死了！水分为 0。" << std::endl;
            break;
        }

        std::string command;
        std::cout << "\n请输入命令 (输入 help 查看帮助): ";
        std::cin >> command;

        for (auto& c : command) c = tolower(c);

        if (command == "help") {
            std::cout << "可用命令:\n"
                << "  find    - 探索资源（输入高度）\n"
                << "  eat     - 吃东西（meat / fish）\n"
                << "  sleep   - 睡觉（恢复生命和水分）\n"
                << "  drink   - 喝水（恢复水分）\n"
                << "  build   - 建造物品（house / bridge / weapon）\n"
                << "  fill    - 用草搓线（消耗3个草获得1根线）\n"
                << "  kill    - 狩猎（消耗1支箭）\n"
                << "  fishing - 钓鱼（需要木棍和线）\n"
                << "  check   - 重新查看状态\n"
                << "  exit / quit - 退出游戏\n";
        }
        else if (command == "find") {
            this->find();
        }
        else if (command == "eat") {
            this->eat();
        }
        else if (command == "sleep") {
            this->sleep();
        }
        else if (command == "drink") {
            this->drink();
        }
        else if (command == "build") {
            this->build();
        }
        else if (command == "fill") {
            this->fill();
        }
        else if (command == "kill") {
            this->kill();
        }
        else if (command == "fishing") {
            this->fishing();
        }
        else if (command == "check") {
            continue;
        }
        else if (command == "exit" || command == "quit") {
            std::cout << "游戏结束，再见！" << std::endl;
            break;
        }
        else {
            std::cout << "未知命令，请重新输入。" << std::endl;
        }

        std::cout << "\n----------------------------------------\n";
    }
}

struct TodoItem {
    int id;
    std::string desc;
    bool completed;
    std::string created;
    TodoItem() : id(0), completed(false) {}
    TodoItem(int i, const std::string& d, bool c = false, const std::string& t = "")
        : id(i), desc(d), completed(c), created(t) {
    }
};

class gamelist {
private:
    int choose = 0, number_s = 0;
    std::string online_gamename, temp_gamename;
    sv_game sg;
public:
    void chooser() {
        // 初始化随机种子（猜数字游戏需要）
        srand(static_cast<unsigned>(time(nullptr)));

        std::cout << "[]=========游戏中心=========[]\n输入1 猜数字游戏\n输入2 启动生存游戏\n输入3 启动对应文件路径的已安装游戏(.exe)" << std::endl << "YOU:";
        std::cin >> choose;
        std::cin.ignore();
        if (choose == 1) {
            int now = std::rand() % 100;
            while (true) {
                std::cout << "猜数字 (0-99): ";
                std::cin >> number_s;
                if (number_s < now) {
                    std::cout << "too small\n";
                }
                else if (number_s > now) {
                    std::cout << "too big\n";
                }
                else {
                    std::cout << "Nice!\n";
                    break;
                }
            }
        }
        else if (choose == 2) {
            this->sg.start();
        }
        else if (choose == 3) {
            std::string path;
            std::cout << "输入路径:" << std::endl;
            std::getline(std::cin, path);
            system(path.c_str());
        }
        else {
            std::cout << "无效选项" << std::endl;
        }
    }
};

class TodoList {
private:
    std::vector<TodoItem> items;
    int next_id;
    std::string data_file;
    bool load_from_file();
    bool save_to_file();
public:
    TodoList(const std::string& filepath = ".\\BiterData\\todo.dat");
    ~TodoList();
    void add(const std::string& description);
    bool remove(int id);
    bool complete(int id);
    void show_all() const;
    void show_details() const;
    void interactive_menu();
};

// ---------- 辅助函数实现 ----------
static void console_clear() {
    system("cls");
}

static std::string current_time_str() {
    time_t now = time(nullptr);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
    return std::string(buf);
}

bool write_file_content(const std::string& path, const std::string& content) {
    std::ofstream ofs(path, std::ios::out | std::ios::binary);
    if (!ofs) return false;
    ofs << content;
    return true;
}

std::string read_file_content(const std::string& path) {
    std::ifstream ifs(path, std::ios::in | std::ios::binary);
    if (!ifs) return "";
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

bool file_exists(const std::string& path) {
    DWORD attr = GetFileAttributesA(path.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
}

std::string get_current_time_str() {
    return current_time_str();
}

std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
}

std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delim))
        tokens.push_back(token);
    return tokens;
}

std::string sha256(const std::string& input) {
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    BYTE hash[32];
    DWORD hash_len = 32;

    if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
        return "";
    if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
        CryptReleaseContext(hProv, 0);
        return "";
    }
    if (!CryptHashData(hHash, (const BYTE*)input.c_str(), (DWORD)input.size(), 0)) {
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        return "";
    }
    if (!CryptGetHashParam(hHash, HP_HASHVAL, hash, &hash_len, 0)) {
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        return "";
    }
    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);

    std::ostringstream oss;
    for (DWORD i = 0; i < hash_len; ++i)
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    return oss.str();
}

std::string get_password(const std::string& prompt) {
    std::cout << prompt;
    std::string pwd;
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode_old, mode_new;
    GetConsoleMode(hStdin, &mode_old);
    mode_new = mode_old & ~ENABLE_ECHO_INPUT;
    SetConsoleMode(hStdin, mode_new);
    std::getline(std::cin, pwd);
    SetConsoleMode(hStdin, mode_old);
    std::cout << "\n";
    return pwd;
}

int safe_get_int(const std::string& prompt, int min_val = INT_MIN, int max_val = INT_MAX) {
    int val;
    while (true) {
        std::cout << prompt;
        if (std::cin >> val) {
            if (val >= min_val && val <= max_val) break;
            else std::cout << "输入超出范围 [" << min_val << ", " << max_val << "]\n";
        }
        else {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << "请输入有效数字\n";
        }
    }
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    return val;
}

// ==================== TodoList 成员函数实现 ====================
TodoList::TodoList(const std::string& filepath) : data_file(filepath), next_id(1) {
    if (!load_from_file()) {
        next_id = 1;
    }
}

TodoList::~TodoList() {
    save_to_file();
}

bool TodoList::load_from_file() {
    std::ifstream in(data_file, std::ios::binary);
    if (!in.is_open()) return false;

    items.clear();
    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);   // 去除可能存在的 \r 以及首尾空白
        if (line.empty()) continue;
        std::vector<std::string> parts;
        std::stringstream ss(line);
        std::string part;
        while (std::getline(ss, part, '|')) {
            parts.push_back(part);
        }
        if (parts.size() < 4) continue;
        try {
            int id = std::stoi(parts[0]);
            bool completed = (std::stoi(parts[2]) != 0);
            TodoItem item(id, parts[1], completed, parts[3]);
            items.push_back(item);
            if (id >= next_id) next_id = id + 1;
        }
        catch (...) {
            // 忽略格式错误行
        }
    }
    in.close();
    return true;
}

bool TodoList::save_to_file() {
    std::ofstream out(data_file, std::ios::binary);
    if (!out.is_open()) return false;
    for (const auto& item : items) {
        out << item.id << '|'
            << item.desc << '|'
            << (item.completed ? 1 : 0) << '|'
            << item.created << '\n';
    }
    out.close();
    return true;
}

void TodoList::add(const std::string& description) {
    if (description.empty()) {
        std::cout << "任务描述不能为空。\n";
        return;
    }
    TodoItem item(next_id++, description, false, current_time_str());
    items.push_back(item);
    save_to_file();
    std::cout << "任务 #" << item.id << " 添加成功。\n";
}

bool TodoList::remove(int id) {
    auto it = std::find_if(items.begin(), items.end(), [id](const TodoItem& item) {
        return item.id == id;
        });
    if (it == items.end()) {
        std::cout << "未找到任务 #" << id << "\n";
        return false;
    }
    items.erase(it);
    save_to_file();
    std::cout << "任务 #" << id << " 已删除。\n";
    return true;
}

bool TodoList::complete(int id) {
    auto it = std::find_if(items.begin(), items.end(), [id](const TodoItem& item) {
        return item.id == id;
        });
    if (it == items.end()) {
        std::cout << "未找到任务 #" << id << "\n";
        return false;
    }
    if (it->completed) {
        std::cout << "任务 #" << id << " 已完成。\n";
        return true;
    }
    it->completed = true;
    save_to_file();
    std::cout << "任务 #" << id << " 标记为已完成。\n";
    return true;
}

void TodoList::show_all() const {
    if (items.empty()) {
        std::cout << "暂无任务。\n";
        return;
    }
    std::cout << "ID  状态  描述\n";
    std::cout << "-------------------\n";
    for (const auto& item : items) {
        std::cout << std::setw(3) << item.id << "  "
            << (item.completed ? "[√]" : "[ ]") << "  "
            << item.desc << '\n';
    }
}

void TodoList::show_details() const {
    if (items.empty()) {
        std::cout << "暂无任务。\n";
        return;
    }
    std::cout << "ID  状态  描述                    创建时间\n";
    std::cout << "--------------------------------------------------------\n";
    for (const auto& item : items) {
        std::cout << std::setw(3) << item.id << "  "
            << (item.completed ? "[√]" : "[ ]") << "  "
            << std::setw(25) << std::left << item.desc
            << "  " << item.created << '\n';
    }
}

void TodoList::interactive_menu() {
    while (true) {
        console_clear();
        std::cout << "============ 待办事项列表 ============\n";
        std::cout << "1. 查看所有任务\n";
        std::cout << "2. 添加任务\n";
        std::cout << "3. 删除任务\n";
        std::cout << "4. 标记完成\n";
        std::cout << "5. 详细查看\n";
        std::cout << "0. 返回\n";
        std::cout << "=======================================\n";
        std::cout << "请选择: ";

        int choice;
        std::cin >> choice;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        if (choice == 0) break;

        switch (choice) {
        case 1:
            show_all();
            break;
        case 2: {
            std::string desc;
            std::cout << "输入任务描述: ";
            std::getline(std::cin, desc);
            add(desc);
            break;
        }
        case 3: {
            int id;
            std::cout << "输入要删除的任务ID: ";
            std::cin >> id;
            std::cin.ignore();
            remove(id);
            break;
        }
        case 4: {
            int id;
            std::cout << "输入要标记完成的任务ID: ";
            std::cin >> id;
            std::cin.ignore();
            complete(id);
            break;
        }
        case 5:
            show_details();
            break;
        default:
            std::cout << "无效选项。\n";
        }
        std::cout << "\n按 Enter 继续...";
        std::cin.get();
    }
}

// ==================== 功能函数 ====================

void Calc(void) {
    console_clear();
    std::cout << "[]================ 计算器 ================[]\n";
    std::cout << "符号: + - * / %\n";
    std::cout << "输入 0 0 退出\n";

    number a, b;
    char op;
    while (true) {
        std::cout << "输入第一个数字: ";
        if (!(std::cin >> a)) {
            std::cout << "Error: Invalid number input!\n";
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }

        std::cout << "输入第二个数字:";
        if (!(std::cin >> b)) {
            std::cout << "Error: Invalid number input!\n";
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }

        if (a == 0 && b == 0) {
            std::cout << "Exit calculator...\n";
            Sleep(800);
            break;
        }

        std::cout << "输入符号:";
        std::cin >> op;

        number res = 0;
        bool err = false;
        switch (op) {
        case '+': res = a + b; break;
        case '-': res = a - b; break;
        case '*': res = a * b; break;
        case '/':
            if (b == 0) {
                std::cout << "Error: Division by zero!\n";
                err = true;
                break;
            }
            res = a / b;
            break;
        case '%': {
            long long ia = (long long)a;
            long long ib = (long long)b;
            if (ib == 0) {
                std::cout << "Error: Mod zero invalid!\n";
                err = true;
                break;
            }
            res = (number)(ia % ib);
            break;
        }
        default:
            std::cout << "Error: Unknown operator!\n";
            err = true;
            break;
        }
        if (!err) {
            std::cout << "结果 = " << res << "\n";
        }
        std::cout << "------------------------------------------------------\n";
    }
}

void TimeClock(void) {
    console_clear();
    std::cout << "[]======== 时钟 ========[]\n";
    std::cout << "按任意键退出时钟\n";

    while (true) {
        if (_kbhit()) break;

        std::time_t curTime = std::time(NULL);
        std::tm* t = std::localtime(&curTime);
        int h = t->tm_hour;
        int m = t->tm_min;
        int s = t->tm_sec;

        COORD pos = { 0,3 };
        SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), pos);
        std::cout << "现在的时间是: "
            << std::setfill('0') << std::setw(2) << h << ":"
            << std::setfill('0') << std::setw(2) << m << ":"
            << std::setfill('0') << std::setw(2) << s;
        Sleep(100);
    }
    std::cin.ignore();
    std::cout << "------------------------------------------------------\n";
}

void SimpleCalendar(void) {
    console_clear();
    std::cout << "[]==================== 日历 ====================[]\n";
    int year, month;
    std::cout << "输入年份>";
    if (!(std::cin >> year)) {
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cout << "无效年份\n";
        std::cout << "按下Enter键返回";
        std::cin.get();
        return;
    }
    std::cout << "输入月份>";
    if (!(std::cin >> month) || month < 1 || month > 12) {
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cout << "无效月份\n";
        std::cout << "按下Enter键返回";
        std::cin.get();
        return;
    }

    std::tm t = { 0 };
    t.tm_year = year - 1900;
    t.tm_mon = month - 1;
    t.tm_mday = 1;
    std::mktime(&t);

    int weekStart = t.tm_wday;
    int dayCount[12] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
    if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))
        dayCount[1] = 29;
    int totalDay = dayCount[month - 1];

    std::cout << "Sun Mon Tue Wed Thu Fri Sat\n";
    for (int i = 0; i < weekStart; i++)
        std::cout << "    ";

    for (int d = 1; d <= totalDay; d++) {
        std::cout << std::setw(3) << d << " ";
        if ((weekStart + d) % 7 == 0)
            std::cout << "\n";
    }
    std::cout << "\n\n";
    std::cout << "按下Enter键返回\n";
    std::cin.ignore();
    std::cin.get();
    std::cout << "------------------------------------------------------\n";
}

void notepad(void) {
    int opt;
    while (true) {
        console_clear();
        std::cout << "================== 记事本 ==================\n";
        std::cout << "1. 开新的一页\n";
        std::cout << "2. 查看已经保存的\n";
        std::cout << "0. 退出\n";
        std::cout << "============================================\n";
        std::cout << YOU;
        std::cin >> opt;
        std::cin.ignore();

        if (opt == 0) break;
        else if (opt == 1) {
            std::ofstream outFile(".\\BiterData\\NoteData.text", std::ios::app);
            if (!outFile.is_open()) {
                std::cout << "找不到保存的记事本\n";
                Sleep(1000);
                continue;
            }
            std::string content;
            bool is_exit_np = false;
            int count = 0;
            while (!is_exit_np) {
                count++;
                std::cout << count << ":";
                std::getline(std::cin, content);
                if (content == ".end")
                    is_exit_np = true;
                else
                    outFile << content << "\n";
            }
            outFile.close();
            std::cout << "文件保存成功\n";
        }
        else if (opt == 2) {
            console_clear();
            std::cout << "============ 查看已经保存的 ============\n";
            std::ifstream inFile(".\\BiterData\\NoteData.text");
            if (!inFile.is_open()) {
                std::cout << "你没有保存文件\n";
            }
            else {
                std::string line;
                while (std::getline(inFile, line)) {
                    std::cout << line << "\n";
                }
                inFile.close();
            }
            std::cout << "------------------------------------------------------\n";
            std::cout << "\n按下Enter键退出";
            std::cin.get();
        }
        Sleep(200);
    }
}

void about(void) {
    console_clear();
    std::cout << "[]==================== 关于 ====================[]\n";
    std::cout << "Biter.ProgramX-Beta3\n";
    std::cout << "开发者: ZengLizard\n";
    std::cout << "版本:" << "Beta3\n";
    std::cout << "发布日期: 2024-06-01\n";
    std::cout << "[]==============================================[]\n";
    std::cout << "\n按下Enter键退出";
    std::cin.get();
}

// ==================== 程序框架 ====================

struct AppContext {
    HWND        tray_hwnd = nullptr;
    HICON       tray_icon = nullptr;
    std::wstring exe_dir;
    std::wstring data_dir;
    std::string  current_user;
    bool         is_exit = false;
};

static AppContext g_app;

std::map<int, std::string> week_table = {
    {1, "Monday"},
    {2, "Tuesday"},
    {3, "Wednesday"},
    {4, "Thursday"},
    {5, "Friday"},
    {6, "Saturday"},
    {7, "Sunday"}
};

static std::wstring get_exe_directory() {
    wchar_t buf[MAX_PATH + 1] = { 0 };
    GetModuleFileNameW(NULL, buf, MAX_PATH);
    wchar_t* last_backslash = wcsrchr(buf, L'\\');
    if (last_backslash != nullptr) {
        *(last_backslash + 1) = L'\0';
    }
    return std::wstring(buf);
}

static bool create_directory_if_not_exist(const std::wstring& path) {
    DWORD attr = GetFileAttributesW(path.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
        return true;
    }
    return CreateDirectoryW(path.c_str(), nullptr) != 0;
}

// ---------- 托盘相关 ----------
static LRESULT CALLBACK TrayUIProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_TRAY_MSG:
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

static bool init_tray_window() {
    if (g_app.tray_hwnd != nullptr)
        return true;
    WNDCLASSEXW wndclass;
    ZeroMemory(&wndclass, sizeof(wndclass));
    wndclass.cbSize = sizeof(WNDCLASSEXW);
    wndclass.lpfnWndProc = TrayUIProc;
    wndclass.hInstance = GetModuleHandleW(NULL);
    wndclass.lpszClassName = L"BiterProgramXTrayClass";

    if (!RegisterClassExW(&wndclass)) {
        return false;
    }

    HWND hwnd = CreateWindowExW(
        0,
        L"BiterProgramXTrayClass",
        L"",
        0,
        0, 0, 0, 0,
        HWND_MESSAGE,
        nullptr,
        GetModuleHandleW(NULL),
        nullptr
    );
    if (!hwnd) {
        UnregisterClassW(L"BiterProgramXTrayClass", GetModuleHandleW(NULL));
        return false;
    }
    g_app.tray_hwnd = hwnd;
    g_app.tray_icon = (HICON)LoadIconW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(1));
    return true;
}

template<size_t N>
static void safe_tstrcpy(TCHAR(&dest)[N], LPCTSTR src) {
    if (src == nullptr) {
        dest[0] = 0;
        return;
    }
    _tcsncpy(dest, src, N - 1);
    dest[N - 1] = 0;
}

static void show_tray_notify(LPCTSTR title, LPCTSTR text) {
    if (g_app.tray_hwnd == nullptr) return;

    NOTIFYICONDATA icondata;
    ZeroMemory(&icondata, sizeof(NOTIFYICONDATA));
    icondata.cbSize = sizeof(NOTIFYICONDATA);
    icondata.hWnd = g_app.tray_hwnd;
    icondata.uID = TIP_ID;
    icondata.uFlags = NIF_ICON | NIF_MESSAGE | NIF_INFO;
    icondata.uCallbackMessage = WM_TRAY_MSG;
    icondata.hIcon = g_app.tray_icon;
    icondata.uTimeout = 30 * 1000;

    safe_tstrcpy(icondata.szInfo, text);
    safe_tstrcpy(icondata.szInfoTitle, title);

    if (!Shell_NotifyIcon(NIM_MODIFY, &icondata)) {
        Shell_NotifyIcon(NIM_ADD, &icondata);
    }
}

static void remove_tray_icon() {
    if (g_app.tray_hwnd == nullptr) return;
    NOTIFYICONDATA icondata;
    ZeroMemory(&icondata, sizeof(NOTIFYICONDATA));
    icondata.cbSize = sizeof(NOTIFYICONDATA);
    icondata.hWnd = g_app.tray_hwnd;
    icondata.uID = TIP_ID;
    Shell_NotifyIcon(NIM_DELETE, &icondata);
}

static void cleanup_tray() {
    remove_tray_icon();
    if (g_app.tray_hwnd) {
        DestroyWindow(g_app.tray_hwnd);
        g_app.tray_hwnd = nullptr;
    }
    if (g_app.tray_icon) {
        DestroyIcon(g_app.tray_icon);
        g_app.tray_icon = nullptr;
    }
    UnregisterClassW(L"BiterProgramXTrayClass", GetModuleHandleW(NULL));
}

class LoginFunction {
public:
    std::string get_current_weekday() {
        time_t now = time(nullptr);
        tm* p_ltm = localtime(&now);
        int wday = p_ltm->tm_wday;
        if (wday == 0) wday = 7;
        return week_table[wday];
    }

    void print_logo() {
        std::cout << "#       ######  #####  #  ##   #\n";
        std::cout << "#       #    #  #         # #  #\n";
        std::cout << "#       #    #  #      #  #  # #\n";
        std::cout << "#       #    #  #   #  #  #   ##\n";
        std::cout << "######  ######  #####  #  #    #\n";
        std::cout << "        LizardBiterXe CO.       \n";
    }

    void prompt_return_console() {
        int result = MessageBox(NULL, TEXT("请返回命令行进行登录"), TEXT("Biter.ProgramX-Beta3"), MB_OKCANCEL | MB_ICONINFORMATION);
        if (result == IDCANCEL) {
            g_app.is_exit = true;
        }
        else {
            console_clear();
        }
    }

    void login(const std::wstring& user_data_filepath, std::string& out_user) {
        ErrorDefine errdef;
        print_logo();
        std::string username;
        std::cout << "请输入用户名:";
        std::getline(std::cin, username);

        FILE* fin = _wfopen(user_data_filepath.c_str(), L"r");
        if (!fin) {
            throw std::runtime_error(std::to_string(errdef.LOGIN_ERROR));
        }
        char buf[1024];
        if (fgets(buf, sizeof(buf), fin) == nullptr) {
            fclose(fin);
            throw std::runtime_error(std::to_string(errdef.LOGIN_ERROR));
        }
        fclose(fin);

        size_t len = strlen(buf);
        while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) {
            buf[len - 1] = 0;
            len--;
        }
        std::string stored_name(buf);

        if (username != stored_name) {
            throw std::runtime_error(std::to_string(errdef.LOGIN_ERROR_NAME_CAN_NOT_FALL_TO_THE_PROGRAM));
        }
        out_user = username;
        std::cout << "登录成功,欢迎使用Biter.ProgramX-Beta3\n";
    }

    void register_user(const std::wstring& user_data_filepath, std::string& out_user) {
        print_logo();
        std::string username;
        std::cout << "请输入用户名:";
        std::getline(std::cin, username);

        FILE* fout = _wfopen(user_data_filepath.c_str(), L"w");
        if (!fout) {
            // 修复：使用 ErrorDefine::REGISTER_ERROR
            throw std::runtime_error(std::to_string(ErrorDefine::REGISTER_ERROR));
        }
        fputs(username.c_str(), fout);
        fputc('\n', fout);
        fclose(fout);

        out_user = username;
        std::cout << "注册成功,欢迎使用Biter.ProgramX-Beta3\n";
    }

    void choose_login_or_register(const std::wstring& user_data_filepath) {
        while (true) {
            console_clear();
            std::cout << "请选择登录或注册:\n";
            std::cout << "1. 登录\n";
            std::cout << "2. 注册\n";
            std::cout << YOU;

            int choice = 0;
            if (!(std::cin >> choice)) {
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::cerr << "无效输入，请输入数字1或2\n";
                Sleep(800);
                continue;
            }
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

            try {
                if (choice == 1) {
                    this->login(user_data_filepath, g_app.current_user);
                    break;
                }
                else if (choice == 2) {
                    this->register_user(user_data_filepath, g_app.current_user);
                    break;
                }
                else {
                    std::cerr << "无效的选择，请输入1或2\n";
                    Sleep(800);
                }
            }
            catch (const std::runtime_error& e) {
                std::cerr << "操作失败,ERCD:" << e.what() << "\n";
                std::cout << "按回车继续...";
                std::cin.get();
            }
        }
    }
};

// ---------- Shell 主循环 ----------
void shell_loop() {
    console_clear();
    std::cout << "#     # #####   ###  #####  ####  \n"
        << "#         #    #   # #   #  #   #\n"
        << "#     #  #     ##### #####  #   #\n"
        << "#     # #      #   # #  #   #   #\n"
        << "##### # #####  #   # #   #  #### \n";
    std::cout << "        LizardBiterXe CO.       \n";
    std::cout << "[]============Shell===========[]\n";
    std::cout << "输入Help或HELP可以获得部分指令使用方法\n";
    std::cout << "检测到你正在使用开发者版本(预览版),有些功能尚未开发完全\n\n";

    while (!g_app.is_exit) {
        std::string prompt = g_app.current_user + ":";
        std::cout << prompt;
        std::string cmdline;
        if (!std::getline(std::cin, cmdline)) {
            break;
        }

        if (cmdline == "exit") {
            show_tray_notify(TEXT("Biter.ProgramX-Beta3"), TEXT("感谢使用Biter ProgramX"));
            g_app.is_exit = true;
        }
        else if (cmdline == "Help" || cmdline == "HELP" || cmdline == "help") {
            show_tray_notify(TEXT("Biter.ProgramX-Beta3"), TEXT("Help菜单里有好东西"));
            std::cout << "[]=========帮助=========[]\n";
            std::cout << " exit          退出程序\n";
            std::cout << " Help          帮助\n";
            std::cout << " clean/clear   清屏\n";
            std::cout << " notepad       记事本\n";
            std::cout << " calendar      日历\n";
            std::cout << " clock/time    时钟\n";
            std::cout << " about         关于\n";
            std::cout << " calc          计算器\n";
            std::cout << " ping          连接测试\n";
            std::cout << " todo          TODO列表\n";
            std::cout << " return        返回命令行\n";
            std::cout << " gamelist      游戏菜单\n";
            std::cout << "[]======================[]\n";
        }
        else if (cmdline == "clean" || cmdline == "clear") {
            console_clear();
        }
        else if (cmdline == "notepad") {
            notepad();
        }
        else if (cmdline == "calendar") {
            SimpleCalendar();
        }
        else if (cmdline == "clock" || cmdline == "time") {
            TimeClock();
        }
        else if (cmdline == "about") {
            about();
        }
        else if (cmdline == "calc") {
            Calc();
        }
        else if (cmdline == "ping") {
            std::string ip;
            std::cout << "PingIP:";
            std::getline(std::cin, ip);
            if (!ip.empty())
                system(("ping " + ip).c_str());
            else
                std::cout << "IP地址不能为空。\n";
        }
        else if (cmdline == "todo") {
            TodoList todo;
            todo.interactive_menu();
        }
        else if (cmdline == "return") {
            std::cout << "输入系统命令，输入 END 退出\n";
            std::string cmd;
            while (true) {
                std::cout << "CMD> ";
                if (!std::getline(std::cin, cmd)) break;
                if (cmd == "END") break;
                if (!cmd.empty())
                    system(cmd.c_str());
            }
        }
        else if (cmdline == "gamelist") {
            gamelist gl;
            gl.chooser();
        }
        else if (cmdline.empty()) {
            continue;
        }
        else {
            std::cerr << "未知指令,看看help\n";
        }
    }
}

static void title(std::string title) {
    system(("title " + title).c_str());
}

LoginFunction logfunc;

// ==================== 主函数 ====================
int main() {
    g_app.exe_dir = get_exe_directory();
    g_app.data_dir = g_app.exe_dir + L"BiterData\\";
    create_directory_if_not_exist(g_app.data_dir);
    std::wstring user_file = g_app.data_dir + L"Biter.Safe.Login.UserData.data";
    title("initing...");
    if (!init_tray_window()) {
        MessageBox(NULL, TEXT("托盘组件初始化失败"), TEXT("警告"), MB_OK | MB_ICONWARNING);
    }
    else {
        show_tray_notify(TEXT("Biter.ProgramX-Beta3"), TEXT("HI~欢迎使用Biter ProgramX"));
    }

    logfunc.prompt_return_console();
    if (g_app.is_exit) {
        cleanup_tray();
        return 0;
    }
    title("loging...");
    logfunc.choose_login_or_register(user_file);
    title("SHELL");
    shell_loop();

    cleanup_tray();
    return 0;
}