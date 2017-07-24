#include <iostream>
#include <fstream>
#include <map>
#include <vector>
#include <limits>
#include <functional>
#include <locale>

// Если перенесимость значения не имеет, можно взять uint64_t
using count_t = unsigned long long;


/** ДКА для разбора URL'ов */
class URLFiniteStateMachine {
public:
    URLFiniteStateMachine() :
        currentState(State::init)
    {};

    /** Анализирует символ, переводя автомат в соответствующее состояние */
    void consume(char ch) {
        switch(currentState) {
            case State::init:
                if('h' == ch) {
                    currentState = State::prefix_h;
                } else {
                    currentState = State::error;
                }
                break;

            case State::prefix_h:
                if('t' == ch) {
                    currentState = State::prefix_t_1;
                } else {
                    currentState = State::error;
                }
                break;

            case State::prefix_t_1:
                if('t' == ch) {
                    currentState = State::prefix_t_2;
                } else {
                    currentState = State::error;
                }
                break;

            case State::prefix_t_2:
                if('p' == ch) {
                    currentState = State::prefix_p;
                } else {
                    currentState = State::error;
                }
                break;

            case State::prefix_p:
                if('s' == ch) {
                    currentState = State::prefix_s;
                } else if(':' == ch) {
                    currentState = State::prefix_colon;
                } else {
                    currentState = State::error;
                }
                break;

            case State::prefix_s:
                if(':' == ch) {
                    currentState = State::prefix_colon;
                } else {
                    currentState = State::error;
                }
                break;

            case State::prefix_colon:
                if('/' == ch) {
                    currentState = State::prefix_slash_1;
                } else {
                    currentState = State::error;
                }
                break;

            case State::prefix_slash_1:
                if('/' == ch) {
                    currentState = State::prefis_slash_2;
                } else {
                    currentState = State::error;
                }
                break;

            case State::prefis_slash_2:
                if(isDomainContent(ch)) {
                    currentState = State::domain_content;
                    addToDomain(ch);
                } else {
                    currentState = State::error;
                }
                break;

            case State::domain_content:
                if(isDomainContent(ch)) {
                    currentState = State::domain_content;
                    addToDomain(ch);
                } else if('/' == ch) {
                    currentState = State::path_slash;
                    path.push_back(ch);
                } else {
                    currentState = State::success;
                    path.push_back('/');
                }
                break;

            case State::path_slash:
                if(isPathContent(ch)) {
                    currentState = State::path_content;
                    path.push_back(ch);
                } else {
                    currentState = State::success;
                }
                break;

            case State::path_content:
                if(isPathContent(ch)) {
                    currentState = State::path_content;
                    path.push_back(ch);
                } else {
                    currentState = State::success;
                }
                break;

            case State::error:
                throw std::invalid_argument("FSM already in error state");
            case State::success:
                throw std::invalid_argument("FSM already in success state");
        }
    }

    void addToDomain(char ch) {
        // Домены не регистрозависимы
        domain.push_back(std::tolower(ch, std::locale()));
    }

    std::string&& takeDomain() {
        return std::move(domain);
    }

    std::string&& takePath() {
        return std::move(path);
    }

    bool isSuccess() const {
        return State::success == currentState;
    }

    bool isError() const {
        return State::error == currentState;
    }

private:
    /** Состояния автомата */
    enum class State {
        init,
        prefix_h,
        prefix_t_1,
        prefix_t_2,
        prefix_p,
        prefix_s,
        prefix_colon,
        prefix_slash_1,
        prefis_slash_2,
        domain_content,
        path_slash,
        path_content,

        error,
        success
    };

    bool isDomainContent(char ch) {
        return isAlpha(ch) || isNumeric(ch) || '.' == ch || '-' == ch;
    }

    bool isPathContent(char ch) {
        return isAlpha(ch) || isNumeric(ch) || '.' == ch || ',' == ch || '/' == ch || '+' == ch || '_' == ch;
    }

    bool isAlpha(char ch) {
        return ('a' <= ch && ch <= 'z') || ('A' <= ch && ch <= 'Z');
    }

    bool isNumeric(char ch) {
        return '0' <= ch && ch <= '9';
    }


    /** Текущее состояние автомата */
    State currentState;
    std::string domain;
    std::string path;
};


/** Разбирает URL'ы (их домены и пути) из потока символов, ведя подсчет встреченных URL'ов */
class URLParser {
public:

    /** Число встреченных неуникальных URL'ов */
    count_t urlCounter;
    /** Число встереченных доменов */
    std::map<std::string, count_t> countByDomain;
    /** Число встреченных путей */
    std::map<std::string, count_t> countByPath;

    URLParser() :
        urlCounter(0),
        countByDomain(),
        countByPath(),
        parsers()
    {};

    /**
     * Обработать следующий символ разбираемой последовательности.
     *
     * @param ch Следующий символ в разбираемой последовательности.
     * @return Домены, разбор которых был полностью завершен на этом шаге.
     */
    void consume(char ch) {
        // Новый символ может быть началом нового URL'а
        parsers.push_back(URLFiniteStateMachine());

        for(auto iter = parsers.begin(); iter != parsers.end(); ) {
            iter->consume(ch);
            if(iter->isSuccess()) {
                ++urlCounter;
                countByDomain[iter->takeDomain()] += 1;
                countByPath[iter->takePath()] += 1;

                iter = parsers.erase(iter);
            } else if(iter->isError()) {
                iter = parsers.erase(iter);
            } else {
                ++iter;
            }
        }
    }

private:
    /** Автоматы, в данный момент не в конечном состоянии */
    std::vector<URLFiniteStateMachine> parsers;
};



void printStats(std::ofstream& out, const std::string& header, const count_t topCount,
                std::map<std::string, count_t>& countByValue);
std::multimap<count_t, std::string, std::greater<count_t>> revertIndex(std::map<std::string, count_t>& countByStr);


int main(int argc, char** argv) {
    if(argc != 3 && argc != 5) {
        std::cerr << "Wrong count of arguments: " << argc << std::endl;
        return EXIT_FAILURE;
    }

    count_t topCount = std::numeric_limits<count_t>::max();
    if(argc == 5) {
        if(argv[1] != std::string("-n")) {
            std::cerr << "Invalid flag " << argv[1] << std::endl;
            return EXIT_FAILURE;
        }

        try {
            topCount = std::stoull(argv[2]);
        } catch(const std::exception& exc) {
            std::cerr << "Invalid count argument " << argv[2] << std::endl;
            return EXIT_FAILURE;
        }
    }

    std::ifstream in(argv[argc - 2]);
    if(!in) {
        std::cerr << "File \"" << argv[argc - 2] << "\" not found" << std::endl;
        return EXIT_FAILURE;
    }

    std::ofstream out(argv[argc - 1]);
    if(!out) {
        std::cerr << "Can't open \"" << argv[argc - 1] << "\" file for output" << std::endl;
        return EXIT_FAILURE;
    }

    URLParser urlParser;

    char ch;
    while(in.get(ch)) {
        urlParser.consume(ch);
    }
    // Завершаем автоматы, находящиеся в корректном незавершенном состоянии разбора, когда поток уже вычитан
    urlParser.consume('\n');

    out << "total urls " << urlParser.urlCounter << ", domains " << urlParser.countByDomain.size()
        << ", paths " << urlParser.countByPath.size() << std::endl;
    printStats(out, "domains", topCount, urlParser.countByDomain);
    printStats(out, "paths", topCount, urlParser.countByPath);

    return EXIT_SUCCESS;
}

void printStats(std::ofstream& out, const std::string& header, const count_t topCount,
                std::map<std::string, count_t>& countByValue)
{
    std::multimap<count_t, std::string, std::greater<count_t>> valuesByCount = revertIndex(countByValue);

    int current = 1;
    out << std::endl << "top " << header << std::endl;
    for(auto iter = valuesByCount.cbegin(); iter != valuesByCount.cend(); ++iter) {
        if(current > topCount) {
            break;
        }

        out << iter->first << " " << iter->second << std::endl;
        ++current;
    }
    valuesByCount.clear();
}

std::multimap<count_t, std::string, std::greater<count_t>> revertIndex(std::map<std::string, count_t>& countByStr) {
    std::multimap<count_t, std::string, std::greater<count_t>> result;
    for(auto iter = countByStr.cbegin(); iter != countByStr.cend(); ) {
        // multimap сохраняет порядок вставки значений по ключю с C++11, поэтому сохраняется лексикографический порядок
        result.emplace(iter->second, iter->first);
        iter = countByStr.erase(iter);
    }

    return result;
}