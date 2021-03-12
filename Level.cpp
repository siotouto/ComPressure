#include "Level.h"

Test::Test()
{
        for (int i = 0; i < HISTORY_POINT_COUNT; i++)
        {
            last_pressure_log[i] = 0;
            best_pressure_log[i] = 0;
        }
        last_pressure_index = 0;

}

void Test::load(SaveObject* sobj)
{
    if (!sobj)
        return;
    SaveObjectMap* omap = sobj->get_map();
    last_score = omap->get_num("last_score");
    best_score = omap->get_num("best_score");

    {
        SaveObjectList* slist = omap->get_item("best_pressure_log")->get_list();
        for (int i = 0; i < HISTORY_POINT_COUNT; i++)
            best_pressure_log[i] = slist->get_num(i);
    }

    {
        SaveObjectList* slist = omap->get_item("last_pressure_log")->get_list();
        for (int i = 0; i < HISTORY_POINT_COUNT; i++)
            last_pressure_log[i] = slist->get_num(i);
    }
    last_pressure_index = omap->get_num("last_pressure_index");
}

SaveObject* Test::save()
{
    SaveObjectMap* omap = new SaveObjectMap;
    omap->add_num("last_score", last_score);
    omap->add_num("best_score", best_score);

    {
        SaveObjectList* slist = new SaveObjectList;
            for (int i = 0; i < HISTORY_POINT_COUNT; i++)
                slist->add_num(best_pressure_log[i]);
        omap->add_item("best_pressure_log", slist);
    }

    {
        SaveObjectList* slist = new SaveObjectList;
            for (int i = 0; i < HISTORY_POINT_COUNT; i++)
                slist->add_num(last_pressure_log[i]);
        omap->add_item("last_pressure_log", slist);
    }
        
    omap->add_num("last_pressure_index", last_pressure_index);

    
    return omap;
}

Level::Level(unsigned level_index_):
    level_index(level_index_)
{
    pin_order[0] = 0; pin_order[1] = 1; pin_order[2] = 2; pin_order[3] = 3;
    circuit = new Circuit;
    init_tests();
    set_monitor_state(monitor_state);
}

Level::Level(unsigned level_index_, SaveObject* sobj):
    level_index(level_index_)
{
    pin_order[0] = 0; pin_order[1] = 1; pin_order[2] = 2; pin_order[3] = 3;
    SaveObjectMap* omap = sobj->get_map();
    circuit = new Circuit(omap->get_item("circuit")->get_map());
    if (omap->has_key("best_design"))
        best_design = new LevelSet(omap->get_item("best_design"), true);
    if (omap->has_key("saved_designs"))
    {
        SaveObjectList* slist = omap->get_item("saved_designs")->get_list();
        for (unsigned i = 0; i < 4; i++)
        {
            if (i < slist->get_count())
            {
                SaveObject *sobj = slist->get_item(i);
                if (!sobj->is_null())
                    saved_designs[i] = new LevelSet(sobj, true);
            }
                
        }
    }

    init_tests(omap);
    set_monitor_state(monitor_state);
}

Level::~Level()
{
    delete circuit;
    delete best_design;
}

SaveObject* Level::save(bool lite)
{
    SaveObjectMap* omap = new SaveObjectMap;
    omap->add_item("circuit", circuit->save());

    if (!lite)
    {
        omap->add_num("level_version", level_version);
        omap->add_num("best_score", best_score);
        omap->add_num("last_score", last_score);
        SaveObjectList* slist = new SaveObjectList;
        unsigned test_count = tests.size();
        for (unsigned i = 0; i < test_count; i++)
            slist->add_item(tests[i].save());
        omap->add_item("tests", slist);
        if (best_design)
            omap->add_item("best_design", best_design->save(level_index));

        slist = new SaveObjectList;
        for (unsigned i = 0; i < 4; i++)
            if (saved_designs[i])
                slist->add_item(saved_designs[i]->save());
            else
                slist->add_item(new SaveObjectNull);
        omap->add_item("saved_designs", slist);

    }
    else
    {
        omap->add_num("best_score", last_score);
    }

    return omap;
}

XYPos Level::getimage(Direction direction)
{
    int mask = connection_mask << direction | connection_mask >> (4 - direction) & 0xF;
    return XYPos(128 + (mask & 0x3) * 32, 32 + ((mask >> 2) & 0x3) * 32);
}

XYPos Level::getimage_fg(Direction direction)
{
    return XYPos(direction * 24, 184 + (int(level_index) * 24));
}


void Level::init_tests(SaveObjectMap* omap)
{
#define CONMASK_N          (1)
#define CONMASK_E          (2)
#define CONMASK_S          (4)
#define CONMASK_W          (8)

#define NEW_TEST do {tests.push_back({}); if (loaded_level_version == level_version && slist && slist->get_count() > tests.size()-1) tests.back().load(slist->get_item(tests.size()-1));} while (false)
#define NEW_POINT(a, b, c, d) tests.back().sim_points.push_back(SimPoint(a, b, c, d, tests.back().tested_direction == DIRECTION_N ? 0 : 50, tests.back().tested_direction == DIRECTION_E ? 0 : 50, tests.back().tested_direction == DIRECTION_S ? 0 : 50, tests.back().tested_direction == DIRECTION_W ? 0 : 50))
#define NEW_POINT_F(a, b, c, d, fa, fb, fc, fd) tests.back().sim_points.push_back(SimPoint(a, b, c, d, fa, fb, fc, fd))

    SaveObjectList* slist = NULL;
    try 
    {
        if (omap)
            slist = omap->get_item("tests")->get_list();
    }
    catch (const std::runtime_error& error)
    {
    }

    unsigned loaded_level_version = 0;
    if (omap && omap->has_key("level_version"))
        loaded_level_version = omap->get_num("level_version");

    switch(level_index)
    {
        case 0:
            name = "Cross";
            connection_mask = CONMASK_N | CONMASK_W | CONMASK_E | CONMASK_S;            // Cross
            pin_order[0] = 3; pin_order[1] = 0; pin_order[2] = 1; pin_order[3] = 2;
            substep_count = 2000;
            level_version = 10;

            NEW_TEST; tests.back().tested_direction = DIRECTION_S;
            NEW_POINT(0  ,  0,  0,  0);
            NEW_POINT(100,  0,100,  0);
            NEW_TEST; tests.back().tested_direction = DIRECTION_S;
            NEW_POINT(100,  0,100,  0);
            NEW_POINT( 50,  0, 50,  0);
            NEW_TEST; tests.back().tested_direction = DIRECTION_S;
            NEW_POINT( 50,  0, 50,  0);
            NEW_POINT(  0,  0,  0,  0);

            NEW_TEST;
            NEW_POINT(  0,  0,  0,  0);
            NEW_POINT(  0,100,  0,100);
            NEW_TEST;
            NEW_POINT(  0,100,  0,100);
            NEW_POINT(  0, 50,  0, 50);
            NEW_TEST;
            NEW_POINT(  0, 50,  0, 50);
            NEW_POINT(  0,  0,  0,  0);
            break;

        case 1:                                                                     // 50
            name = "50";
            connection_mask = CONMASK_E;
            substep_count = 2000;
            level_version = 10;
            NEW_TEST;
            NEW_POINT(  0, 50,  0,  0);
            NEW_TEST;
            NEW_POINT(  0, 50,  0,  0);
            NEW_TEST;
            NEW_POINT(  0, 50,  0,  0);
            NEW_TEST;
            NEW_POINT(  0, 50,  0,  0);
            break;

        case 2:                                                                     // NPN
            name = "NPN";
            connection_mask = CONMASK_N | CONMASK_W | CONMASK_E;
            substep_count = 2000;
            level_version = 10;

            NEW_TEST;// N   E   S   W
            NEW_POINT(100,  0,  0,  0);
            NEW_POINT(100, 50,  0, 50);
            NEW_TEST;
            NEW_POINT(100, 50,  0, 50);
            NEW_POINT(100,100,  0,100);
            NEW_TEST;
            NEW_POINT(100,100,  0,100);
            NEW_POINT(  0,100,  0,100);
            NEW_POINT(  0,100,  0, 50);
            NEW_TEST;
            NEW_POINT(  0,100,  0, 50);
            NEW_POINT(  0,100,  0,  0);
            NEW_TEST;
            NEW_POINT(  0,100,  0,  0);
            NEW_POINT(100, 50,  0, 50);
            break;

        case 3:                                                                     // PNP
            name = "PNP";
            connection_mask = CONMASK_S | CONMASK_W | CONMASK_E;
            substep_count = 2000;
            level_version = 10;

            NEW_TEST;// N   E   S   W
            NEW_POINT(  0,  0,  0,  0);
            NEW_POINT(  0, 50,  0, 50);
            NEW_TEST;
            NEW_POINT(  0, 50,  0, 50);
            NEW_POINT(  0,100,  0,100);
            NEW_TEST;
            NEW_POINT(  0,100,  0,100);
            NEW_POINT(  0,100,100,100);
            NEW_POINT(  0,100,100, 50);
            NEW_TEST;
            NEW_POINT(  0,100,100, 50);
            NEW_POINT(  0,100,100,  0);
            NEW_TEST;
            NEW_POINT(  0,100,100,  0);
            NEW_POINT(  0, 50,  0, 50);
            break;

        case 4:                                                                     // select
            name = "Switch";
            connection_mask = CONMASK_N | CONMASK_W | CONMASK_E | CONMASK_S;
            substep_count = 7000;
            level_version = 10;

            NEW_TEST;// N   E   S   W
            NEW_POINT(  0,  0,  0,  0);
            NEW_POINT(  0,100,100,  0);
            NEW_TEST;
            NEW_POINT(  0,100,100,  0);
            NEW_POINT(  0, 50, 50,  0);
            NEW_TEST;
            NEW_POINT(  0, 50, 50,  0);
            NEW_POINT(  0,  0, 50,100);
            NEW_TEST;
            NEW_POINT(  0,  0, 50,100);
            NEW_POINT(100,100, 50,100);
            NEW_TEST;
            NEW_POINT(100,100,  0,100);
            NEW_POINT(100, 50,  0, 50);
            NEW_TEST;
            NEW_POINT(100, 50,  0, 50);
            NEW_POINT(  0, 50,100, 50);
            NEW_TEST;
            NEW_POINT(  0, 50,100, 50);
            NEW_POINT( 20, 40, 60, 50);
            NEW_TEST;
            NEW_POINT( 20, 40, 60, 50);
            NEW_POINT(100,  0,  0,  0);
            NEW_TEST;
            NEW_POINT(100,  0,  0,  0);
            NEW_POINT(100,100,  0,100);
            NEW_TEST;
            NEW_POINT(100,100,  0,100);
            NEW_POINT( 40, 40,  0,100);
            NEW_TEST;
            NEW_POINT( 40, 40,  0,100);
            NEW_POINT(  0,  0,  0,  0);
            break;


        case 5:                                                                 // Buf
            name = "Buffer";
            connection_mask = CONMASK_W | CONMASK_E;
            level_version = 11;

            circuit->force_element(XYPos(3,0), new CircuitElementEmpty());
            circuit->force_element(XYPos(3,1), new CircuitElementEmpty());
            circuit->force_element(XYPos(4,2), new CircuitElementEmpty());
            circuit->force_element(XYPos(5,3), new CircuitElementValve(DIRECTION_E));
            circuit->force_element(XYPos(4,4), new CircuitElementEmpty());
            circuit->force_element(XYPos(5,5), new CircuitElementValve(DIRECTION_W));
            circuit->force_element(XYPos(4,6), new CircuitElementEmpty());
            circuit->force_element(XYPos(3,7), new CircuitElementEmpty());
            circuit->force_element(XYPos(3,8), new CircuitElementEmpty());
            NEW_TEST;// N   E   S   W
            NEW_POINT(  0,  0,  0,  0);
            NEW_POINT(  0,  0,  0,  0);
            NEW_TEST;
            NEW_POINT(  0,  0,  0,  0);
            NEW_POINT(  0, 10,  0, 10);
            NEW_TEST;
            NEW_POINT(  0, 10,  0, 10);
            NEW_POINT(  0, 50,  0, 50);
            NEW_TEST;
            NEW_POINT(  0, 50,  0, 50);
            NEW_POINT(  0, 90,  0, 90);
            NEW_TEST;
            NEW_POINT(  0, 90,  0, 90);
            NEW_POINT(  0,100,  0,100);
            NEW_TEST;
            NEW_POINT(  0,100,  0,100);
            NEW_POINT(  0, 90,  0, 90);
            NEW_TEST;
            NEW_POINT(  0, 90,  0, 90);
            NEW_POINT(  0, 50,  0, 50);
            NEW_TEST;
            NEW_POINT(  0, 50,  0, 50);
            NEW_POINT(  0, 20,  0, 20);
            NEW_TEST;
            NEW_POINT(  0, 20,  0, 20);
            NEW_POINT(  0,  0,  0,  0);
            NEW_TEST;
            NEW_POINT(  0,  0,  0,  0);
            NEW_POINT(  0, 50,  0, 50);
            NEW_TEST;
            NEW_POINT(  0, 50,  0, 50);
            NEW_POINT(  0,100,  0,100);
            NEW_TEST;
            NEW_POINT(  0,100,  0,100);
            NEW_POINT(  0,  0,  0,  0);
            break;


        case 6:                                                                     // Inv
            name = "Inverter";
            connection_mask = CONMASK_W | CONMASK_E;
            level_version = 10;

            NEW_TEST;// N   E   S   W
            NEW_POINT(  0,100,  0,  0);
            NEW_POINT(  0,100,  0,  0);
            NEW_TEST;
            NEW_POINT(  0,100,  0,  0);
            NEW_POINT(  0, 90,  0, 10);
            NEW_TEST;
            NEW_POINT(  0, 90,  0, 10);
            NEW_POINT(  0, 50,  0, 50);
            NEW_TEST;
            NEW_POINT(  0, 50,  0, 50);
            NEW_POINT(  0, 40,  0, 60);
            NEW_TEST;
            NEW_POINT(  0, 40,  0, 60);
            NEW_POINT(  0, 10,  0, 90);
            NEW_TEST;
            NEW_POINT(  0, 10,  0, 90);
            NEW_POINT(  0,  0,  0,100);
            NEW_TEST;
            NEW_POINT(  0,  0,  0,100);
            NEW_POINT(  0, 50,  0, 50);
            NEW_TEST;
            NEW_POINT(  0, 50,  0, 50);
            NEW_POINT(  0,100,  0,  0);
            NEW_TEST;
            NEW_POINT(  0,100,  0,  0);
            NEW_POINT(  0, 50,  0, 50);
            NEW_TEST;
            NEW_POINT(  0, 50,  0, 50);
            NEW_POINT(  0,100,  0,  0);
            NEW_TEST;
            NEW_POINT(  0,100,  0,  0);
            NEW_POINT(  0,  0,  0,100);
            NEW_TEST;
            NEW_POINT(  0,  0,  0,100);
            NEW_POINT(  0,100,  0,  0);
            break;

        case 7:                                                             // op amp
            name = "Comparator";
            connection_mask = CONMASK_N | CONMASK_S | CONMASK_E;
            level_version = 10;

            NEW_TEST;// N   E   S   W
            NEW_POINT(  0,  0,100,  0);
            NEW_POINT(  0,  0,100,  0);
            NEW_TEST;
            NEW_POINT(  0,  0,100,  0);
            NEW_POINT(100,100,  0,  0);
            NEW_TEST;
            NEW_POINT(100,100,  0,  0);
            NEW_POINT( 10,  0, 90,  0);
            NEW_TEST;
            NEW_POINT( 10,  0, 90,  0);
            NEW_POINT( 80,100, 20,  0);
            NEW_TEST;
            NEW_POINT( 80,100, 20,  0);
            NEW_POINT( 30,  0, 70,  0);
            NEW_TEST;
            NEW_POINT( 30,  0, 70,  0);
            NEW_POINT( 60,100, 40,  0);
            NEW_TEST;
            NEW_POINT( 60,100, 40,  0);
            NEW_POINT( 20,  0, 30,  0);
            NEW_TEST;
            NEW_POINT( 20,  0, 30,  0);
            NEW_POINT( 80,100, 70,  0);
            NEW_TEST;
            NEW_POINT( 80,100, 70,  0);
            NEW_POINT( 40,  0, 50,  0);
            NEW_TEST;
            NEW_POINT( 40,  0, 50,  0);
            NEW_POINT( 55,100, 50,  0);
            NEW_TEST;
            NEW_POINT( 55,100, 50,  0);
            NEW_POINT( 55,  0, 60,  0);
            NEW_TEST;
            NEW_POINT( 55,  0, 60,  0);
            NEW_POINT(100,100, 95,  0);
            break;

        case 8:                                                                 // amp
            name = "Amp50";
            connection_mask = CONMASK_W | CONMASK_E;
            level_version = 10;

            NEW_TEST;// N   E   S   W
            NEW_POINT(  0,  0,  0,  0);
            NEW_POINT(  0,  0,  0,  0);
            NEW_TEST;
            NEW_POINT(  0,  0,  0,  0);
            NEW_POINT(  0,100,  0,100);
            NEW_TEST;
            NEW_POINT(  0,100,  0,100);
            NEW_POINT(  0,  0,  0, 10);
            NEW_TEST;
            NEW_POINT(  0,  0,  0, 10);
            NEW_POINT(  0,100,  0, 90);
            NEW_TEST;
            NEW_POINT(  0,100,  0, 90);
            NEW_POINT(  0,  0,  0, 20);
            NEW_TEST;
            NEW_POINT(  0,  0,  0, 20);
            NEW_POINT(  0,100,  0, 80);
            NEW_TEST;
            NEW_POINT(  0,100,  0, 80);
            NEW_POINT(  0,  0,  0, 30);
            NEW_TEST;
            NEW_POINT(  0,  0,  0, 30);
            NEW_POINT(  0,100,  0, 70);
            NEW_TEST;
            NEW_POINT(  0,100,  0, 70);
            NEW_POINT(  0,  0,  0, 40);
            NEW_TEST;
            NEW_POINT(  0,  0,  0, 40);
            NEW_POINT(  0,100,  0, 60);
            break;

        case 9:                                                        // div 2
            name = "Div2";
            connection_mask = CONMASK_W | CONMASK_E;
            level_version = 10;

            NEW_TEST;// N   E   S   W
            NEW_POINT_F(  0,  0,  0,  0, 50, 0, 50, 50);
            NEW_POINT_F(  0,  0,  0,  0, 50, 0, 50, 50);
            NEW_TEST;
            NEW_POINT_F(  0,  0,  0,  0, 50, 0, 50, 80);
            NEW_POINT_F(  0, 50,  0,100, 50, 0, 50, 80);
            NEW_TEST;
            NEW_POINT_F(  0, 50,  0,100, 50, 0, 50, 40);
            NEW_POINT_F(  0, 30,  0, 60, 50, 0, 50, 50);
            NEW_TEST;
            NEW_POINT_F(  0, 30,  0, 60, 50, 0, 50,100);
            NEW_POINT_F(  0, 25,  0, 50, 50, 0, 50,100);
            NEW_TEST;
            NEW_POINT_F(  0, 25,  0, 50, 50, 0, 50, 60);
            NEW_POINT_F(  0, 20,  0, 40, 50, 0, 50, 60);
            NEW_TEST;
            NEW_POINT_F(  0, 20,  0, 40, 50, 0, 50, 80);
            NEW_POINT_F(  0, 10,  0, 20, 50, 0, 50, 80);
            NEW_TEST;
            NEW_POINT_F(  0, 10,  0, 20, 50, 0, 50, 40);
            NEW_POINT_F(  0, 25,  0, 50, 50, 0, 50, 40);
            NEW_TEST;
            NEW_POINT_F(  0, 25,  0, 50, 50, 0, 50, 60);
            NEW_POINT_F(  0, 50,  0,100, 50, 0, 50, 60);
            NEW_TEST;
            NEW_POINT_F(  0, 50,  0,100, 50, 0, 50, 40);
            NEW_POINT_F(  0,  0,  0,  0, 50, 0, 50, 40);
            NEW_TEST;
            NEW_POINT_F(  0,  0,  0,  0, 50, 0, 50, 50);
            NEW_POINT_F(  0, 50,  0,100, 50, 0, 50, 50);
            NEW_TEST;
            NEW_POINT_F(  0, 50,  0,100, 50, 0, 50, 40);
            NEW_POINT_F(  0,  0,  0,  0, 50, 0, 50, 40);
            NEW_TEST;
            NEW_POINT_F(  0,  0,  0,  0, 50, 0, 50, 50);
            NEW_POINT_F(  0, 50,  0,100, 50, 0, 50, 50);
            NEW_TEST;
            NEW_POINT_F(  0, 50,  0,100, 50, 0, 50, 20);
            NEW_POINT_F(  0,  0,  0,  0, 50, 0, 50, 20);
            break;

        case 10:                                                        // mul 2
            name = "Mul2";
            connection_mask = CONMASK_W | CONMASK_E;
            level_version = 10;

            NEW_TEST;// N   E   S   W
            NEW_POINT(  0,  0,  0,  0);
            NEW_POINT(  0,  0,  0,  0);
            NEW_TEST;
            NEW_POINT(  0,  0,  0,  0);
            NEW_POINT(  0,100,  0, 50);
            NEW_TEST;
            NEW_POINT(  0,100,  0, 50);
            NEW_POINT(  0, 80,  0, 40);
            NEW_TEST;
            NEW_POINT(  0, 80,  0, 40);
            NEW_POINT(  0, 60,  0, 30);
            NEW_TEST;
            NEW_POINT(  0, 60,  0, 30);
            NEW_POINT(  0, 40,  0, 20);
            NEW_TEST;
            NEW_POINT(  0, 40,  0, 20);
            NEW_POINT(  0, 20,  0, 10);
            NEW_TEST;
            NEW_POINT(  0, 20,  0, 10);
            NEW_POINT(  0, 50,  0, 25);
            NEW_TEST;
            NEW_POINT(  0, 50,  0, 25);
            NEW_POINT(  0,  0,  0,  0);
            break;

        case 11:                                                        // Encrypt
            name = "Encrypt";
            connection_mask = CONMASK_W | CONMASK_E;
            level_version = 11;
            
            circuit->force_element(XYPos(0,1), new CircuitElementEmpty());
            circuit->force_element(XYPos(1,1), new CircuitElementEmpty());
            circuit->force_element(XYPos(2,1), new CircuitElementEmpty());
            circuit->force_element(XYPos(0,2), new CircuitElementPipe(CONNECTIONS_ES));
            circuit->force_element(XYPos(1,2), new CircuitElementValve(DIRECTION_E));
            circuit->force_element(XYPos(2,2), new CircuitElementSource(DIRECTION_W));
            circuit->force_element(XYPos(0,3), new CircuitElementPipe(CONNECTIONS_NES));
            circuit->force_element(XYPos(1,3), new CircuitElementValve(DIRECTION_E));
            circuit->force_element(XYPos(2,3), new CircuitElementSource(DIRECTION_W));
            circuit->force_element(XYPos(0,4), new CircuitElementPipe(CONNECTIONS_NWS));
            circuit->force_element(XYPos(1,4), new CircuitElementPipe(CONNECTIONS_NES));
            circuit->force_element(XYPos(2,4), new CircuitElementPipe(CONNECTIONS_EW));
            circuit->force_element(XYPos(0,5), new CircuitElementPipe(CONNECTIONS_NE));
            circuit->force_element(XYPos(1,5), new CircuitElementValve(DIRECTION_W));
            circuit->force_element(XYPos(2,5), new CircuitElementEmpty());
            circuit->force_element(XYPos(0,6), new CircuitElementEmpty());
            circuit->force_element(XYPos(1,6), new CircuitElementSource(DIRECTION_N));
            circuit->force_element(XYPos(2,6), new CircuitElementEmpty());
            circuit->force_element(XYPos(0,7), new CircuitElementEmpty());
            circuit->force_element(XYPos(1,7), new CircuitElementEmpty());
            circuit->force_element(XYPos(2,7), new CircuitElementEmpty());

            NEW_TEST;// N   E   S   W
            NEW_POINT(  0,  0,  0,  0);
            NEW_POINT(  0,  0,  0,  0);
            NEW_TEST;
            NEW_POINT(  0,  0,  0,  0);
            NEW_POINT(  0, 24,  0, 13);
            NEW_TEST;
            NEW_POINT(  0, 24,  0, 13);
            NEW_POINT(  0, 27,  0, 15);
            NEW_TEST;
            NEW_POINT(  0, 27,  0, 15);
            NEW_POINT(  0, 64,  0, 48);
            NEW_TEST;
            NEW_POINT(  0, 64,  0, 48);
            NEW_POINT(  0, 69,  0, 54);
            NEW_TEST;
            NEW_POINT(  0, 69,  0, 54);
            NEW_POINT(  0, 83,  0, 73);
            NEW_TEST;
            NEW_POINT(  0, 83,  0, 73);
            NEW_POINT(  0, 85,  0, 76);
            NEW_TEST;
            NEW_POINT(  0, 85,  0, 76);
            NEW_POINT(  0, 87,  0, 79);
            NEW_TEST;
            NEW_POINT(  0, 87,  0, 79);
            NEW_POINT(  0, 94,  0, 90);
            NEW_TEST;
            NEW_POINT(  0, 94,  0, 90);
            NEW_POINT(  0, 97,  0, 95);
            NEW_TEST;
            NEW_POINT(  0, 97,  0, 95);
            NEW_POINT(  0,100,  0,100);
            break;


        case 12:                                                        // Decrypt
            name = "Decrypt";
            connection_mask = CONMASK_W | CONMASK_E;
            level_version = 11;
            substep_count = 20000;
            circuit->force_element(XYPos(3,4), new CircuitElementSubCircuit(DIRECTION_N, 7));

            circuit->force_element(XYPos(4,5), new CircuitElementSubCircuit(DIRECTION_S, 11));

            NEW_TEST;// N   E   S   W
            NEW_POINT(  0,  0,  0,  0);
            NEW_POINT(  0, 13,  0, 24);
            NEW_TEST;
            NEW_POINT(  0, 13,  0, 24);
            NEW_POINT(  0, 15,  0, 27);
            NEW_TEST;
            NEW_POINT(  0, 15,  0, 27);
            NEW_POINT(  0, 48,  0, 64);
            NEW_TEST;
            NEW_POINT(  0, 48,  0, 64);
            NEW_POINT(  0, 54,  0, 69);
            NEW_TEST;
            NEW_POINT(  0, 54,  0, 69);
            NEW_POINT(  0, 73,  0, 83);
            NEW_TEST;
            NEW_POINT(  0, 73,  0, 83);
            NEW_POINT(  0, 76,  0, 85);
            NEW_TEST;
            NEW_POINT(  0, 76,  0, 85);
            NEW_POINT(  0, 79,  0, 87);
            NEW_TEST;
            NEW_POINT(  0, 79,  0, 87);
            NEW_POINT(  0, 90,  0, 94);
            NEW_TEST;
            NEW_POINT(  0, 90,  0, 94);
            NEW_POINT(  0, 95,  0, 97);


            break;

        case 13:                                                                 // Sensor
            name = "Sensor";
            connection_mask = CONMASK_W | CONMASK_E;
            level_version = 11;

            circuit->force_element(XYPos(0,3), new CircuitElementSource(DIRECTION_S));
            circuit->force_element(XYPos(0,4), new CircuitElementValve(DIRECTION_W));
            circuit->force_element(XYPos(0,5), new CircuitElementPipe(CONNECTIONS_NES));
            circuit->force_element(XYPos(0,6), new CircuitElementValve(DIRECTION_E));
            circuit->force_element(XYPos(0,7), new CircuitElementEmpty());
            circuit->force_element(XYPos(1,4), new CircuitElementEmpty());

            NEW_TEST;// N   E   S   W
            NEW_POINT(  0,  0,  0,  0);
            NEW_POINT(  0,  0,  0,  0);
            NEW_TEST;
            NEW_POINT(  0,  0,  0,  0);
            NEW_POINT(  0, 10,  0, 10);
            NEW_TEST;
            NEW_POINT(  0, 10,  0, 10);
            NEW_POINT(  0, 50,  0, 50);
            NEW_TEST;
            NEW_POINT(  0, 50,  0, 50);
            NEW_POINT(  0, 60,  0, 60);
            NEW_TEST;
            NEW_POINT(  0, 60,  0, 60);
            NEW_POINT(  0, 20,  0, 20);
            NEW_TEST;
            NEW_POINT(  0, 20,  0, 20);
            NEW_POINT(  0, 70,  0, 70);
            NEW_TEST;
            NEW_POINT(  0, 70,  0, 70);
            NEW_POINT(  0, 50,  0, 50);
            NEW_TEST;
            NEW_POINT(  0, 50,  0, 50);
            NEW_POINT(  0, 20,  0, 20);
            NEW_TEST;
            NEW_POINT(  0, 20,  0, 20);
            NEW_POINT(  0,  0,  0,  0);
            NEW_TEST;
            NEW_POINT(  0,  0,  0,  0);
            NEW_POINT(  0, 50,  0, 50);
            NEW_TEST;
            NEW_POINT(  0, 50,  0, 50);
            NEW_POINT(  0, 30,  0, 30);
            NEW_TEST;
            NEW_POINT(  0, 30,  0, 30);
            NEW_POINT(  0,  0,  0,  0);
            break;

        case 14:                                                    // subtract
            name = "Subtractor";
            substep_count = 30000;
            connection_mask = CONMASK_W | CONMASK_S | CONMASK_E;
            level_version = 11;
            pin_order[0] = 3; pin_order[1] = 2; pin_order[2] = 1; pin_order[3] = 0;
            NEW_TEST;// N   E   S   W
            NEW_POINT(  0,  0,  0,  0);
            NEW_POINT(  0,  0,  0,  0);
            NEW_TEST;
            NEW_POINT(  0,  0,  0,  0);
            NEW_POINT(  0, 40, 50, 90);
            NEW_TEST;
            NEW_POINT(  0, 40, 50, 90);
            NEW_POINT(  0, 50, 20, 70);
            NEW_TEST;
            NEW_POINT(  0, 50, 20, 70);
            NEW_POINT(  0,  0, 70, 70);
            NEW_TEST;
            NEW_POINT(  0,  0, 70, 70);
            NEW_POINT(  0, 25, 25, 50);
            NEW_TEST;
            NEW_POINT(  0, 25, 25, 50);
            NEW_POINT(  0, 50, 20, 70);
            NEW_TEST;
            NEW_POINT(  0, 50, 20, 70);
            NEW_POINT(  0, 20, 20, 40);
            NEW_TEST;
            NEW_POINT(  0, 20, 20, 40);
            NEW_POINT(  0, 40,  0, 40);
            NEW_TEST;
            NEW_POINT(  0, 40,  0, 40);
            NEW_POINT(  0,  0, 40, 40);
            NEW_TEST;
            NEW_POINT(  0,  0, 40, 40);
            NEW_POINT(  0, 50, 50,100);
            NEW_TEST;
            NEW_POINT(  0, 50, 50,100);
            NEW_POINT(  0,100,  0,100);
            break;


        case 15:                                                    // add
            name = "Adder";
            substep_count = 30000;
            connection_mask = CONMASK_N | CONMASK_S | CONMASK_E;
            level_version = 10;

            NEW_TEST;// N   E   S   W
            NEW_POINT(  0,  0,  0,  0);
            NEW_POINT( 20, 20,  0,  0);
            NEW_TEST;
            NEW_POINT( 20, 20,  0,  0);
            NEW_POINT( 70, 80, 10,  0);
            NEW_TEST;
            NEW_POINT( 70, 80, 10,  0);
            NEW_POINT( 40, 80, 40,  0);
            NEW_TEST;
            NEW_POINT( 40, 80, 40,  0);
            NEW_POINT( 50, 70, 20,  0);
            NEW_TEST;
            NEW_POINT( 50, 70, 20,  0);
            NEW_POINT( 70, 80, 10,  0);
            NEW_TEST;
            NEW_POINT( 70, 80, 10,  0);
            NEW_POINT( 25, 75, 50,  0);
            NEW_TEST;
            NEW_POINT( 25, 75, 50,  0);
            NEW_POINT(  0, 50, 50,  0);
            NEW_TEST;
            NEW_POINT(  0, 50, 50,  0);
            NEW_POINT( 10, 80, 70,  0);
            NEW_TEST;
            NEW_POINT( 10, 80, 70,  0);
            NEW_POINT( 80, 90, 10,  0);
            NEW_TEST;
            NEW_POINT( 80, 90, 10,  0);
            NEW_POINT( 20, 40, 20,  0);
            NEW_TEST;
            NEW_POINT( 20, 40, 20,  0);
            NEW_POINT( 60,100, 40,  0);
            NEW_TEST;
            NEW_POINT( 60,100, 40,  0);
            NEW_POINT( 20, 80, 60,  0);
            NEW_TEST;
            NEW_POINT( 20, 80, 60,  0);
            NEW_POINT(  0,  0,  0,  0);
            break;
 
        case 16:                                                    // latch
            name = "Latch";
            connection_mask = CONMASK_W | CONMASK_S | CONMASK_E;
            level_version = 10;
            NEW_TEST;// N   E   S   W
            NEW_POINT(  0,  0,  0,  0);
            NEW_POINT(  0,  0,100,  0);
            NEW_POINT(  0,  0,  0,  0);
            NEW_TEST;
            NEW_POINT(  0,  0,100, 40);
            NEW_POINT(  0, 40,  0, 40);
            NEW_TEST;
            NEW_POINT(  0,  0,100, 80);
            NEW_POINT(  0, 80,  0, 80);
            NEW_POINT(  0, 80,  0,  0);
            NEW_TEST;
            NEW_POINT(  0,  0,100, 60);
            NEW_POINT(  0, 60,  0, 60);
            NEW_POINT(  0, 60,100, 30);
            NEW_TEST;
            NEW_POINT(  0, 60,100, 30);
            NEW_POINT(  0, 30,  0, 30);
            NEW_POINT(  0, 30,  0, 90);
            NEW_TEST;
            NEW_POINT(  0, 60,100, 90);
            NEW_POINT(  0, 90,  0, 90);
            NEW_TEST;
            NEW_POINT(  0, 90,100, 40);
            NEW_POINT(  0, 40,  0, 40);
            NEW_TEST;
            NEW_POINT(  0, 40,100,  0);
            NEW_POINT(  0,  0,  0,  0);
            NEW_TEST;
            NEW_POINT(  0,  0,100,100);
            NEW_POINT(  0,100,  0,100);
            break;

        case 17:                                                    // tabulator
            name = "Tabulator";
            substep_count = 30000;
            connection_mask = CONMASK_N | CONMASK_W | CONMASK_S | CONMASK_E;
            level_version = 10;
            NEW_TEST;// N   E   S   W
            NEW_POINT(  0,  0,  0, 10);
            NEW_POINT(  0,  0,100, 10);
            NEW_POINT(  0, 10,  0, 10);
            NEW_TEST;
            NEW_POINT(  0, 10,  0, 10);
            NEW_POINT(  0, 10,100, 10);
            NEW_POINT(  0, 20,  0, 10);
            NEW_TEST;
            NEW_POINT(  0, 20,  0,  0);
            NEW_POINT(  0, 20,100,  0);
            NEW_POINT(  0, 20,  0,  0);
            NEW_TEST;
            NEW_POINT(  0, 20,  0, 60);
            NEW_POINT(  0, 20,100, 60);
            NEW_POINT(  0, 80,  0, 60);
            NEW_TEST;
            NEW_POINT(100, 80,  0, 40);
            NEW_POINT(100, 80,100, 40);
            NEW_POINT(100, 40,  0, 40);
            NEW_TEST;
            NEW_POINT(100, 40,  0, 40);
            NEW_POINT(100, 40,100, 40);
            NEW_POINT(100, 00,  0, 40);
            NEW_TEST;
            NEW_POINT(  0,  0,  0, 30);
            NEW_POINT(  0,  0,100, 30);
            NEW_POINT(  0, 30,  0, 30);
            NEW_TEST;
            NEW_POINT(  0, 30,  0, 50);
            NEW_POINT(  0, 30,100, 50);
            NEW_POINT(  0, 80,  0, 50);
            NEW_TEST;
            NEW_POINT(  0, 80,  0, 20);
            NEW_POINT(  0, 80,100, 20);
            NEW_POINT(  0,100,  0, 20);
            NEW_TEST;
            NEW_POINT(100,100,  0, 50);
            NEW_POINT(100,100,100, 50);
            NEW_POINT(100, 50,  0, 50);
            NEW_TEST;
            NEW_POINT(100, 50,  0,  0);
            NEW_POINT(100, 50,100,  0);
            NEW_POINT(100, 50,  0,  0);
            NEW_TEST;
            NEW_POINT(100, 50,  0, 50);
            NEW_POINT(100, 50,100, 50);
            NEW_POINT(100,  0,  0, 50);
            break;

        case 18:                                                    // broken link
            name = "Broken Link";
            connection_mask = CONMASK_W | CONMASK_E;
            substep_count = 50000;
            level_version = 10;
            circuit->force_element(XYPos(0,4), new CircuitElementPipe(CONNECTIONS_WS));
            circuit->force_element(XYPos(0,5), new CircuitElementPipe(CONNECTIONS_NE));
            circuit->force_element(XYPos(1,5), new CircuitElementPipe(CONNECTIONS_EW));
            circuit->force_element(XYPos(2,5), new CircuitElementPipe(CONNECTIONS_EW));
            circuit->force_element(XYPos(3,5), new CircuitElementPipe(CONNECTIONS_EW));
            circuit->force_element(XYPos(4,5), new CircuitElementPipe(CONNECTIONS_EW));
            circuit->force_element(XYPos(5,5), new CircuitElementPipe(CONNECTIONS_NS_WE));
            circuit->force_element(XYPos(6,5), new CircuitElementPipe(CONNECTIONS_EW));
            circuit->force_element(XYPos(7,5), new CircuitElementValve(DIRECTION_N));
            circuit->force_element(XYPos(7,4), new CircuitElementSource(DIRECTION_S));
            circuit->force_element(XYPos(8,5), new CircuitElementPipe(CONNECTIONS_NWS));
            circuit->force_element(XYPos(8,4), new CircuitElementPipe(CONNECTIONS_ES));

            circuit->force_element(XYPos(0,6), new CircuitElementEmpty());
            circuit->force_element(XYPos(1,6), new CircuitElementPipe(CONNECTIONS_ES));
            circuit->force_element(XYPos(2,6), new CircuitElementPipe(CONNECTIONS_EW));
            circuit->force_element(XYPos(3,6), new CircuitElementPipe(CONNECTIONS_EWS));
            circuit->force_element(XYPos(4,6), new CircuitElementPipe(CONNECTIONS_EW));
            circuit->force_element(XYPos(5,6), new CircuitElementPipe(CONNECTIONS_NWS));
            circuit->force_element(XYPos(6,6), new CircuitElementEmpty());
            circuit->force_element(XYPos(7,6), new CircuitElementPipe(CONNECTIONS_NES));
            circuit->force_element(XYPos(8,6), new CircuitElementValve(DIRECTION_W));

            circuit->force_element(XYPos(0,7), new CircuitElementSource(DIRECTION_E));
            circuit->force_element(XYPos(1,7), new CircuitElementValve(DIRECTION_S));
            circuit->force_element(XYPos(2,7), new CircuitElementPipe(CONNECTIONS_EW));
            circuit->force_element(XYPos(3,7), new CircuitElementValve(DIRECTION_N));
            circuit->force_element(XYPos(4,7), new CircuitElementPipe(CONNECTIONS_EW));
            circuit->force_element(XYPos(5,7), new CircuitElementValve(DIRECTION_S));
            circuit->force_element(XYPos(6,7), new CircuitElementPipe(CONNECTIONS_EW));
            circuit->force_element(XYPos(7,7), new CircuitElementValve(DIRECTION_W));
            circuit->force_element(XYPos(8,7), new CircuitElementPipe(CONNECTIONS_EWS));

            circuit->force_element(XYPos(0,8), new CircuitElementSource(DIRECTION_E));
            circuit->force_element(XYPos(1,8), new CircuitElementPipe(CONNECTIONS_ALL));
            circuit->force_element(XYPos(2,8), new CircuitElementPipe(CONNECTIONS_EW));
            circuit->force_element(XYPos(3,8), new CircuitElementPipe(CONNECTIONS_NWE));
            circuit->force_element(XYPos(4,8), new CircuitElementPipe(CONNECTIONS_EW));
            circuit->force_element(XYPos(5,8), new CircuitElementPipe(CONNECTIONS_NW));
            circuit->force_element(XYPos(6,8), new CircuitElementEmpty());
            circuit->force_element(XYPos(7,8), new CircuitElementSource(DIRECTION_N));
            circuit->force_element(XYPos(8,8), new CircuitElementSource(DIRECTION_N));

            NEW_TEST;// N   E   S   W
            NEW_POINT_F(  0,  0,  0,100,  0,  0,  0, 50);
            break;

        case 19:                                                    // end
            name = "Not yet done...";
            connection_mask = CONMASK_W | CONMASK_E;
            NEW_TEST;// N   E   S   W
            NEW_POINT(  0,  0,  0, 77);
            break;

//         case 10:                                                             // amp inv
//             connection_mask = CONMASK_W | CONMASK_E;
// 
//             NEW_TEST;// N   E   S   W
//             NEW_POINT(  0,  0,  0,  0);
//             NEW_POINT(  0,  0,  0,100);
//             NEW_TEST;
//             NEW_POINT(  0,  0,  0,100);
//             NEW_POINT(  0,100,  0, 10);
//             NEW_TEST;
//             NEW_POINT(  0,100,  0, 10);
//             NEW_POINT(  0,  0,  0, 90);
//             NEW_TEST;
//             NEW_POINT(  0,  0,  0, 90);
//             NEW_POINT(  0,100,  0, 20);
//             NEW_TEST;
//             NEW_POINT(  0,100,  0, 20);
//             NEW_POINT(  0,  0,  0, 80);
//             NEW_TEST;
//             NEW_POINT(  0,  0,  0, 80);
//             NEW_POINT(  0,100,  0, 30);
//             NEW_TEST;
//             NEW_POINT(  0,100,  0, 30);
//             NEW_POINT(  0,  0,  0, 70);
//             NEW_TEST;
//             NEW_POINT(  0,  0,  0, 70);
//             NEW_POINT(  0,100,  0, 40);
//             NEW_TEST;
//             NEW_POINT(  0,100,  0, 40);
//             NEW_POINT(  0,  0,  0, 60);
//             break;
// 
// 
// 
//         case 12:                                                    // zoom amplify
//             substep_count = 30000;
//             connection_mask = CONMASK_N | CONMASK_W | CONMASK_E | CONMASK_S;
// 
//             NEW_TEST;// N   E   S   W
//             NEW_POINT(100,  0,  0,  0);
//             NEW_POINT(100,  0,  0,  0);
//             NEW_TEST;
//             NEW_POINT(100,  0,  0,  0);
//             NEW_POINT(100,100,  0,100);
//             NEW_TEST;
//             NEW_POINT(100,100,  0,100);
//             NEW_POINT(100,  0, 50, 50);
//             NEW_TEST;
//             NEW_POINT(100,  0, 50, 50);
//             NEW_POINT( 90,100, 50, 80);
//             NEW_TEST;
//             NEW_POINT( 90,100, 50, 80);
//             NEW_POINT( 90,  0, 50, 60);
//             NEW_TEST;
//             NEW_POINT( 90,  0, 50, 60);
//             NEW_POINT( 80,100, 20, 60);
//             NEW_TEST;
//             NEW_POINT( 80,100, 20, 60);
//             NEW_POINT( 80,  0, 20, 40);
//             NEW_TEST;
//             NEW_POINT( 80,  0, 20, 40);
//             NEW_POINT( 80,100, 30, 60);
//             NEW_TEST;
//             NEW_POINT( 80,100, 30, 60);
//             NEW_POINT(100,  0,  0, 45);
//             NEW_TEST;
//             NEW_POINT(100,  0,  0, 45);
//             NEW_POINT(100,100,  0, 55);
//             NEW_TEST;
//             NEW_POINT(100,100,  0, 55);
//             NEW_POINT( 60,100, 45, 55);
//             NEW_TEST;
//             NEW_POINT( 60,100, 45, 55);
//             NEW_POINT( 60,  0, 45, 50);
//             break;
// 
        default:
            printf("no level %d\n", level_index);
            break;
    }

    if (loaded_level_version == level_version && omap)
    {
        if (omap->has_key("best_score"))
            best_score = omap->get_num("best_score");
        if (omap->has_key("last_score"))
            last_score = omap->get_num("last_score");
    }
}

void Level::reset(LevelSet* level_set)
{
    test_index = 0;
    sim_point_index = 0;
    substep_index = 0;
    touched = false;

    circuit->elaborate(level_set);
    circuit->reset();
    for (int i = 0; i < 4; i++)
        ports[i] = 0;
}

void Level::advance(unsigned ticks)
{
    static int val = 0;
    for (int tick = 0; tick < ticks; tick++)
    {
        for (int p = 0; p < 4; p++)
            ports[p].pre();

        circuit->sim_pre(PressureAdjacent(ports[0], ports[1], ports[2], ports[3]));
        for (int p = 0; p < 4; p++)
        {
            if ((((connection_mask >> p) & 1) && p != tests[test_index].tested_direction) || (monitor_state == MONITOR_STATE_PAUSE))
                ports[p].apply(current_simpoint.values[p], current_simpoint.force[p]);
        }
        circuit->sim_post(PressureAdjacent(ports[0], ports[1], ports[2], ports[3]));

        for (int p = 0; p < 4; p++)
            ports[p].post();

        if (sim_point_index == tests[test_index].sim_points.size() - 1)
        {
            Pressure value = ports[tests[test_index].tested_direction].value;
            unsigned index = (substep_index * HISTORY_POINT_COUNT) / substep_count;
            tests[test_index].last_pressure_log[index] = value;
            tests[test_index].last_pressure_index = index + 1;
        }

        if (monitor_state != MONITOR_STATE_PAUSE)
        {
            substep_index++;
            if (substep_index >= substep_count)
            {
                substep_index  = 0;
                if (sim_point_index == tests[test_index].sim_points.size() - 1)
                {
                    Direction p = tests[test_index].tested_direction;
                    Pressure score = percent_as_pressure(25) - abs(percent_as_pressure(current_simpoint.values[p]) - ports[p].value) * 100;
                    if (score < 0)
                        score /= 2;
                    score += percent_as_pressure(25);
                    if (score < 0)
                        score /= 2;
                    score += percent_as_pressure(25);
                    if (score < 0)
                        score /= 2;
                    score += percent_as_pressure(25);
                    if (score < 0)
                        score = 0;
                    tests[test_index].last_score = score;
                    update_score(false);
                    sim_point_index = 0;
                    substep_index = 0;
                    if (monitor_state == MONITOR_STATE_PLAY_ALL)
                    {
                        test_index++;
                        if (test_index == tests.size())
                        {
                            if (!touched)
                                update_score(true);
                            test_index = 0;
                            circuit->reset();
                            touched = false;
                        }
                    }
                }
                else
                {
                    sim_point_index++;
                }
                current_simpoint = tests[test_index].sim_points[sim_point_index];
            }
        }

        if (test_pressure_histroy_sample_downcounter <= 0 )
        {
            test_pressure_histroy_sample_downcounter = substep_count / 200;

            for (int p = 0; p < 4; p++)
                test_pressure_histroy[test_pressure_histroy_index].values[p] = ports[p].value;
            test_pressure_histroy_index = (test_pressure_histroy_index + 1) % 192;

        }
        test_pressure_histroy_sample_downcounter--;
    }
}

void Level::select_test(unsigned t)
{
    if (t >= tests.size())
        return;
    sim_point_index = 0;
    substep_index = 0;
    test_index = t;
    touched = true;
}

void Level::update_score(bool fin)
{
    unsigned test_count = tests.size();
    Pressure score = 0;
    for (unsigned i = 0; i < test_count; i++)
    {
        score += tests[i].last_score;
    }
    score /= test_count;
    last_score = score;
    if ((score > best_score || !best_design) && fin)
    {
        best_score = score;
        for (unsigned t = 0; t < test_count; t++)
        {
            tests[t].best_score = tests[t].last_score;
            for (int i = 0; i < HISTORY_POINT_COUNT; i++)
                tests[t].best_pressure_log[i] = tests[t].last_pressure_log[i];
        }
        best_score_set = true;
    }
    if (fin)
        score_set = true;
    return;
}

void Level::set_monitor_state(TestExecType monitor_state_)
{
    monitor_state = monitor_state_;
    if (monitor_state != MONITOR_STATE_PAUSE)
        current_simpoint = tests[test_index].sim_points[sim_point_index];
}

LevelSet::LevelSet(SaveObject* sobj, bool inspect)
{
    read_only = inspect;
    SaveObjectList* slist = sobj->get_list();
    for (int i = 0; i < LEVEL_COUNT; i++)
    {
        SaveObject *sobj = NULL;
        if (i < slist->get_count())
        {
            sobj = slist->get_item(i);
            if (sobj->is_null())
                sobj = NULL;
        }
        if (sobj)
            levels[i] = new Level(i, sobj);
        else
        {
            if (inspect)
                levels[i] = NULL;
            else
                levels[i] = new Level(i);
        }
    }
}

LevelSet::LevelSet()
{
        for (int i = 0; i < LEVEL_COUNT; i++)
        {
            levels[i] = new Level(i);
        }
}

LevelSet::~LevelSet()
{
        for (int i = 0; i < LEVEL_COUNT; i++)
        {
            delete levels[i];
        }
}

SaveObject* LevelSet::save(bool lite)
{
    SaveObjectList* slist = new SaveObjectList;
    
    for (int i = 0; i < LEVEL_COUNT; i++)
    {
        if (levels[i])
            slist->add_item(levels[i]->save(lite));
        else
            slist->add_item(new SaveObjectNull);
    }
    return slist;
}

SaveObject* LevelSet::save(unsigned level_index)
{
    SaveObjectList* slist = new SaveObjectList;
    
    for (int i = 0; i < LEVEL_COUNT; i++)
    {
        if (levels[level_index]->circuit->contains_subcircuit_level(i, this) || i == level_index)
            slist->add_item(levels[i]->save(true));
        else
            slist->add_item(new SaveObjectNull);
    }
    return slist;
}

bool LevelSet::is_playable(unsigned level)
{
    if (level >= LEVEL_COUNT)
        return false;
    if (read_only)
        return (levels[level] != NULL);
    for (int i = 0; i < level; i++)
    {
        if (levels[i]->best_score < percent_as_pressure(75))
            return false;
    }
    return true;
}

int LevelSet::top_playable()
{
    if (read_only)
        return -1;
    for (int i = 0; i < LEVEL_COUNT; i++)
    {
        if (levels[i]->best_score < percent_as_pressure(75))
            return i;
    }
    return LEVEL_COUNT - 1;
}

Pressure LevelSet::test_level(unsigned level_index)
{
    levels[level_index]->reset(this);
    levels[level_index]->set_monitor_state(MONITOR_STATE_PLAY_ALL);
    while (!levels[level_index]->score_set)
        levels[level_index]->advance(1000);
    return levels[level_index]->last_score;
}

void LevelSet::record_best_score(unsigned level_index)
{
    SaveObject* sobj = save(level_index);
    delete levels[level_index]->best_design;
    levels[level_index]->best_design =  new LevelSet(sobj, true);
    delete sobj;
}

void LevelSet::save_design(unsigned level_index, unsigned save_slot)
{
    SaveObject* sobj = save(level_index);
    delete levels[level_index]->saved_designs[save_slot];
    levels[level_index]->saved_designs[save_slot] =  new LevelSet(sobj, true);
    delete sobj;
}

