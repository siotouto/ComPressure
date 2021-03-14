#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <list>
#include <stdexcept>
#include <signal.h>

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "Compress.h"
#include "SaveState.h"
#include "Level.h"

class Database;

class Score
{
public:
    Pressure score = 0;
    SaveObject* sobj = NULL;
    Score(){}
    Score(const Score& other)
    {
        score = other.score;
        if (other.sobj)
            sobj = other.sobj->dup();
    }
    ~Score()
    {
        delete sobj;
    }
    void update_design(SaveObject* sobj_)
    {
        delete sobj;
        sobj = sobj_;
    }
};
    

class ScoreTable
{
public:
    std::multimap<Pressure, uint64_t, std::greater<Pressure>> sorted_scores;
    std::map<uint64_t, Score> user_score;
    
    ~ScoreTable()
    {
   
    }
    
    SaveObject* save()
    {
        SaveObjectList* score_list = new SaveObjectList;
        for(auto const &score : sorted_scores)
        {
            SaveObjectMap* score_map = new SaveObjectMap;
            uint64_t id = score.second;
            score_map->add_num("id", id);
            score_map->add_num("score", user_score[id].score);
            score_map->add_item("design", user_score[id].sobj->dup());
            score_list->add_item(score_map);
        }
        return score_list;
    }
    
    void load(SaveObject* sobj)
    {
        SaveObjectList* score_list = sobj->get_list();
        for (unsigned i = 0; i < score_list->get_count(); i++)
        {
            SaveObjectMap* omap = score_list->get_item(i)->get_map();
            add_score(omap->get_num("id"), omap->get_num("score"), omap->get_item("design"));
        }
    }

    Pressure get_score(uint64_t steam_id)
    {
        return user_score[steam_id].score;
    }

    void add_score(uint64_t steam_id, Pressure score, SaveObject* sobj)
    {
//        if (!steam_id)
//            return;
        if (score < user_score[steam_id].score)
            return;
            
        if (score == user_score[steam_id].score)
        {
            user_score[steam_id].update_design(sobj->dup());
            return;
        }

        for (auto it = sorted_scores.begin(); it != sorted_scores.end(); )
        {
           if (it->second == steam_id)
           {
                if (it->first >= score)
                    return;
                it = sorted_scores.erase(it);
                break;
           }
           else
               ++it;
        }
        sorted_scores.insert({score, steam_id});
        user_score[steam_id].score = score;
        user_score[steam_id].update_design(sobj->dup());
    }
    void fetch_scores(SaveObjectMap* omap, std::set<uint64_t>& friends, Database& db);

};

class Player
{
public:
    std::string steam_username;
    bool priv;
};

class Database
{
public:
    std::map<uint64_t, Player> players;
    std::vector<ScoreTable> levels;

    void update_name(uint64_t steam_id, std::string& steam_username)
    {
        players[steam_id].steam_username = steam_username;
    }

    void update_priv(uint64_t steam_id, bool priv)
    {
        players[steam_id].priv = priv;
    }

    void update_score(uint64_t steam_id, unsigned level, Pressure score, SaveObject* sobj)
    {
        if (level >= levels.size())
            levels.resize(level + 1);
        levels[level].add_score(steam_id, score, sobj);
    }
    void load(SaveObject* sobj)
    {
        SaveObjectMap* omap = sobj->get_map();
        SaveObjectList* name_list = omap->get_item("names")->get_list();
        for (unsigned i = 0; i < name_list->get_count(); i++)
        {
            SaveObjectMap* omap = name_list->get_item(i)->get_map();
            std::string name;
            uint64_t id = omap->get_num("id");
            omap->get_string("steam_username", name);
            bool priv = omap->get_num("priv");
            update_name(id, name);
            update_priv(id, priv);
        }
;
        SaveObjectList* level_list = omap->get_item("levels")->get_list();
        if (level_list->get_count() > levels.size())
            levels.resize(level_list->get_count());
        for (unsigned i = 0; i < level_list->get_count(); i++)
        {
            SaveObject* sobj = level_list->get_item(i);
            levels[i].load(sobj);
        }

    }

    SaveObject* save()
    {
        SaveObjectMap* omap = new SaveObjectMap;
        
        SaveObjectList* name_list = new SaveObjectList;
        
        for(auto const &player_pair : players)
        {
            SaveObjectMap* name_map = new SaveObjectMap;
            name_map->add_num("id", player_pair.first);
            name_map->add_num("priv", player_pair.second.priv);
            name_map->add_string("steam_username", player_pair.second.steam_username);
            name_list->add_item(name_map);
        }
        omap->add_item("names", name_list);
        
        
        
        unsigned highest = levels.size();
        SaveObjectList* level_list = new SaveObjectList;
        for (int i = 0; i < highest; i++)
        {
            level_list->add_item(levels[i].save());
        }
        omap->add_item("levels", level_list);

        return omap;
    }

    SaveObject* get_scores(unsigned level, uint64_t user_id, std::set<uint64_t>& friends)
    {
        if (level >= levels.size())
            levels.resize(level + 1);
        SaveObjectMap* omap = new SaveObjectMap;
        omap->add_num("level", level);
        omap->add_num("score", levels[level].get_score(user_id));
        levels[level].fetch_scores(omap, friends, *this);
        return omap;
    }

};



class Workload
{
public:
    virtual ~Workload(){};
    virtual bool execute() = 0;
};

class SubmitScore : public Workload
{
public:
    LevelSet* level_set;
    int current_level = 0;
    bool init_level = false;
    std::string steam_username;
    uint64_t steam_id;
    Database& db;
    
    SubmitScore(Database& db_, SaveObjectMap* omap):
        db(db_)
    {
        level_set = new LevelSet(omap->get_item("levels"), true);
        omap->get_string("steam_username", steam_username);
        steam_id = omap->get_num("steam_id");
        db.update_name(steam_id, steam_username);
    }

    ~SubmitScore()
    {
        delete level_set;
    }
    void update_scores()
    {
        for (unsigned level_index = 0; level_index < LEVEL_COUNT; level_index++)
        {
            if (level_set->is_playable(level_index))
            {
                Pressure score = level_set->levels[level_index]->last_score;
                SaveObject* save_object = level_set->save(level_index);
                db.update_score(steam_id, level_index, score, save_object);
                delete save_object;
            }
        }
    }

    bool execute()
    {
        while (!level_set->is_playable(current_level))
        {
            current_level++;
            if (current_level >= LEVEL_COUNT)
            {
                update_scores();
                return true;
            }
        }
        if (!init_level)
        {
            level_set->reset(current_level);
            level_set->levels[current_level]->last_score = 0;
            level_set->levels[current_level]->best_score = 0;
            init_level = true;
        }
        level_set->levels[current_level]->advance(10000);
        if (level_set->levels[current_level]->score_set)
        {
            current_level++;
            init_level = false;
        }
        return false;
    }
};


class Connection
{
public:
    int conn_fd;
    int length;
    std::string inbuf;
    std::string outbuf;
    Connection(int conn_fd_):
        conn_fd(conn_fd_),
        length(-1)
    {
    }

    Workload* recieve(Database& db)
    {
        Workload* resp = NULL;
        static char buf[1024];
        while (true)
        {
            ssize_t num_bytes_received = recv(conn_fd, buf, sizeof(buf), MSG_DONTWAIT);
            if (num_bytes_received  == -1)
            {
                if (errno != EAGAIN && errno != EWOULDBLOCK)
                    close();
                break;
            }
            else if (num_bytes_received  == 0)
            {
                close();
                break;
            }
            inbuf.append(buf, num_bytes_received);
        }

        while (true)
        {
            ssize_t num_bytes_received = send(conn_fd, outbuf.c_str(), outbuf.length(), MSG_DONTWAIT);
            if (num_bytes_received  == -1)
            {
                if (errno != EAGAIN && errno != EWOULDBLOCK)
                    close();
                break;
            }
            else if (num_bytes_received  == 0)
            {
                break;
            }
            outbuf.erase(0, num_bytes_received);
        }
        while (true)
        {
            if (length < 0 && inbuf.length() >= 4)
            {
                length = *(uint32_t*)inbuf.c_str(),
                inbuf.erase(0, 4);
                if (length > 1024*1024)
                {
                    close();
                    break;
                }
            }
            else if (length > 0 && inbuf.length() >= length)
            {
                try
                {
                    std::string decomp = decompress_string(inbuf);
                    std::istringstream decomp_stream(decomp);
                    SaveObjectMap* omap = SaveObject::load(decomp_stream)->get_map();
                    inbuf.erase(0, length);
                    length = -1;

                    std::string command;
                    omap->get_string("command", command);
                    if (command == "save")
                    {
                        std::string steam_username;
                        omap->get_string("steam_username", steam_username);
                        db.update_name(omap->get_num("steam_id"), steam_username);
                        printf("save: %s %lld\n", steam_username.c_str(), omap->get_num("steam_id"));
                        std::ofstream outfile (steam_username.c_str());
                        omap->get_item("content")->save(outfile);
                        outfile.close();
                        if (0)
                        {
                            SaveObjectMap* score_map = new SaveObjectMap;
                            score_map->add_item("levels", omap->get_item("content")->get_map()->get_item("levels")->dup());
                            score_map->add_num("steam_id", omap->get_num("steam_id"));
                            score_map->add_string("steam_username", steam_username);
                            resp = new SubmitScore(db, score_map);
                            delete score_map;
                        }
                    }
                    else if (command == "score_submit")
                    {
                        std::string steam_username;
                        omap->get_string("steam_username", steam_username);
                        db.update_name(omap->get_num("steam_id"), steam_username);
                        printf("score_submit: %s %lld\n", steam_username.c_str(), omap->get_num("steam_id"));
                        resp = new SubmitScore(db, omap);
                    }
                    else if (command == "score_fetch")
                    {
                        std::string steam_username;
                        omap->get_string("steam_username", steam_username);
                        db.update_name(omap->get_num("steam_id"), steam_username);
                        printf("score_fetch: %s %lld\n", steam_username.c_str(), omap->get_num("steam_id"));
                        unsigned level = omap->get_num("level");
                        std::set<uint64_t> friends;
                        
                        if (omap->has_key("friends"))
                        {
                            SaveObjectList* friend_list = omap->get_item("friends")->get_list();
                            for (unsigned i = 0; i < friend_list->get_count(); i++)
                            {
                                friends.insert(friend_list->get_num(i));
                            }
                            friends.insert(omap->get_num("steam_id"));
                        }
                        SaveObject* scores = db.get_scores(level, omap->get_num("steam_id"), friends);
                        std::ostringstream stream;
                        scores->save(stream);
                        std::string comp = compress_string(stream.str());
                        uint32_t length = comp.length();
                        outbuf.append((char*)&length, 4);
                        outbuf.append(comp);
                        delete scores;
                    }
                    delete omap;
                }
                catch (const std::runtime_error& error)
                {
                    std::cerr << error.what() << "\n";
                    close();
                    break;
                }
            }
            else
                break;
        }
        return resp;
    }
    
    void close()
    {
        if (conn_fd < 0)
            return;
        printf("closed %d\n", conn_fd);
        shutdown(conn_fd, SHUT_WR);
        ::close(conn_fd);
        conn_fd = -1;
    }

};

bool power_down = false;

void sig_handler(int signo)
{
    if (signo == SIGUSR1)
        printf("received SIGUSR1\n");
    else if (signo == SIGTERM)
        printf("received SIGTERM\n");
    power_down = true;
    printf("shutting down\n");
}

int main()
{
    Database db;
    signal(SIGUSR1, sig_handler);
    signal(SIGINT,  sig_handler);
    signal(SIGTERM, sig_handler);

    try 
    {
        std::ifstream loadfile("db.save");
        if (!loadfile.fail() && !loadfile.eof())
        {
            SaveObjectMap* omap = SaveObject::load(loadfile)->get_map();
            db.load(omap);
            delete omap;
        }
    }
    catch (const std::runtime_error& error)
    {
        std::cerr << error.what() << "\n";
    }




    struct sockaddr_in myaddr ,clientaddr;
    int sockid;
    sockid=socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK , 0);
//    memset(&myaddr,'0',sizeof(myaddr));
    myaddr.sin_family=AF_INET;
    myaddr.sin_port=htons(42069);
    myaddr.sin_addr.s_addr=INADDR_ANY;
    if(sockid==-1)
    {
        perror("socket");
    }
    socklen_t len=sizeof(myaddr);
    if(bind(sockid,( struct sockaddr*)&myaddr,len)==-1)
    {
        perror("bind");
    }
    if(listen(sockid,10)==-1)
    {
        perror("listen");
    }
    
    
    std::list<Connection> conns;
    std::list<Workload*> workloads;

    time_t old_time = 0;

    while(true)
    {
        if (workloads.empty())
        {
            fd_set w_fds;
            fd_set r_fds;
            struct timeval timeout;
            timeout.tv_sec = 5;
            timeout.tv_usec = 0;

            FD_ZERO(&r_fds);
            FD_ZERO(&w_fds);
            FD_SET(sockid, &r_fds);
            
            for (Connection &conn :conns)
            {
                FD_SET(conn.conn_fd, &r_fds);
                if (!conn.outbuf.empty())
                    FD_SET(conn.conn_fd, &w_fds);
            }
            
            select(1024, &r_fds, &w_fds, NULL, &timeout);
        }

        while (true)
        {
            int conn_fd = accept(sockid,(struct sockaddr *)&clientaddr, &len);
            if (conn_fd == -1)
                break;
            printf("new: %d \n", conn_fd);
            if (conn_fd >= 1024)
                ::close(conn_fd);
            conns.push_back(conn_fd);
        }

        for (std::list<Connection>::iterator it = conns.begin(); it != conns.end();)
        {
            Connection& conn = (*it);
            Workload* new_workload = conn.recieve(db);
            if (new_workload)
            {
                workloads.push_back(new_workload);
            }
            if (conn.conn_fd < 0)
            {
                it = conns.erase(it);
            }
            else
                it++;
        }

        for (std::list<Workload*>::iterator it = workloads.begin();it != workloads.end();)
        {
            Workload* workload = (*it);
            if (workload->execute())
            {
                delete workload;
                it = workloads.erase(it);
            }
            else
                it++;
        }

        fflush(stdout);
        if (power_down)
            break;

        time_t new_time;
        time(&new_time);
        if ((old_time + 60) < new_time)
        {
            old_time = new_time;
            std::ofstream outfile ("db.save");
            SaveObject* savobj = db.save();
            savobj->save(outfile);
            delete savobj;
        }
    }
    close(sockid);
    {
        std::ofstream outfile ("db.save");
        SaveObject* savobj = db.save();
        savobj->save(outfile);
        delete savobj;
    }
    return 0;
}

void ScoreTable::fetch_scores(SaveObjectMap* omap, std::set<uint64_t>& friends, Database& db)
{
    SaveObjectList* friend_scores = new SaveObjectList;

    std::vector<Pressure> scores;
    for(auto const &score : sorted_scores)
    {
        scores.push_back(score.first);
        if (friends.find(score.second) != friends.end())
        {
            SaveObjectMap* omap = new SaveObjectMap;
            omap->add_num("steam_id", score.second);
            omap->add_string("steam_username", db.players[score.second].steam_username);
            omap->add_num("score", score.first);
            friend_scores->add_item(omap);
        }
    }

    SaveObjectList* score_list = new SaveObjectList;

    unsigned count = scores.size();
    for (unsigned i = 0; i < 200; i++)
    {
        Pressure result = 0;
        if (count)
            result = scores[(i * count) / 200];
        score_list->add_num(result);

    }
    omap->add_item("graph", score_list);
    omap->add_item("friend_scores", friend_scores);
}
