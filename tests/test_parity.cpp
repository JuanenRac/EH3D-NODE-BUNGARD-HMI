// EH3D-NODE-BUNGARD-HMI - host tests of what the screen shares with the desktop program: the bar of the tool changer (places, chaining), the card of the work in hand, the stages
// of a job that drills and routes, the panel that moves the machine by hand and the settings of all of them.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "../core/gate.hpp"
#include "../core/hmi_config.hpp"
#include "../core/job_card.hpp"
#include "../core/jog_panel.hpp"
#include "../core/notices.hpp"
#include "../core/rack.hpp"
#include "../core/stage_notice.hpp"
#include "../core/status.hpp"
#include "../core/workflow.hpp"

static int g_checks = 0;
static int g_failed = 0;
#define CHECK(c)                                                \
    do {                                                        \
        ++g_checks;                                             \
        if (!(c)) {                                             \
            ++g_failed;                                         \
            std::printf("FAILED line %d: %s\n", __LINE__, #c); \
        }                                                       \
    } while (0)

using namespace hmi;

static bool close_to(double a, double b) { return std::fabs(a - b) < 1e-9; }
static bool has(const std::string& s, const char* part) { return s.find(part) != std::string::npos; }

static Response answer(int status, const std::string& body) { return Response{status, body}; }

static FleetView fleet_with(const std::string& job_id, const std::string& model, int done = 2, int total = 5, const std::string& extra = "") {
    const std::string job = job_id.empty() ? std::string("null")
                                           : "{\"id\":\"" + job_id + "\",\"name\":\"board\",\"percent\":40,\"holes_done\":" + std::to_string(done) + ",\"holes_total\":" + std::to_string(total) + extra + "}";
    FleetView v;
    parse_fleet("{\"time_s\":1800000000,\"machines\":[{\"id\":\"m1\",\"name\":\"M\",\"model\":\"" + model + "\",\"state\":\"" + std::string(job_id.empty() ? "idle" : "busy") + "\",\"job\":" + job + "}]}", v);
    return v;
}

// a drilling file with tools 1 (0.6 mm), 2 (0.8 mm) and 3 (1.0 mm), used in the order 2, 1, 3, and a routing file with the pens 1 and 2
static const char* kDrill = "M48\nMETRIC\nT1C0.6\nT2C0.8\nT3C1.0\n%\nT2\nX1Y1\nX2Y2\nT1\nX3Y3\nT3\nX4Y4\nM30\n";
static const char* kRoute = "IN;SP1;PU0,0;PD40,0,40,40;PU;SP2;PU10,10;PD20,20;PU;";

// ---- the places of the bar --------------------------------------------------------------------------------------------------------------

static void places_of_the_bar() {
    std::vector<int> v;
    CHECK(parse_places("1-14", v) && v.size() == 14 && v.front() == 1 && v.back() == 14);
    CHECK(parse_places("1, 2,5-8", v) && v.size() == 6 && v[2] == 5 && v[5] == 8);
    CHECK(parse_places("3,3,2", v) && v.size() == 2 && v[0] == 2);
    CHECK(parse_places("0,15,16,4", v) && v.size() == 1 && v[0] == 4);  // 0 is the test pin and 15 the detector: not places for a tool
    for (const char* bad : {"", " ", "a", "1-", "-3", "5-2", "1,,2", "1,", "1 2"}) CHECK(!parse_places(bad, v));

    const std::vector<int> defined = {1, 2, 3, 4, 5};
    const std::vector<RackTool> tools = {{false, 1, 0.6}, {false, 2, 0.8}, {true, 1, 0}};
    RackAssignment a;
    // nothing given: every tool has no place
    std::vector<RackProblem> p = check_rack(tools, a, defined);
    CHECK(p.size() == 3 && p[0].kind == RackProblem::Kind::Unassigned && p[2].route && p[2].tool == 1);
    // two in one place, one in a place the machine does not have
    set_slot(a, false, 1, 2);
    set_slot(a, false, 2, 2);
    set_slot(a, true, 1, 9);
    p = check_rack(tools, a, defined);
    CHECK(p.size() == 2 && p[0].kind == RackProblem::Kind::Shared && p[0].tool == 2 && p[0].other_tool == 1 && p[0].slot == 2);
    CHECK(p[1].kind == RackProblem::Kind::Undefined && p[1].route && p[1].slot == 9);
    // taking a place away and giving another
    set_slot(a, false, 2, 0);
    CHECK(slot_of(a, false, 2) == 0 && slot_of(a, false, 1) == 2);
    set_slot(a, false, 2, 4);
    set_slot(a, true, 1, 5);
    CHECK(check_rack(tools, a, defined).empty());

    // the tools without a place take the free ones from the lowest: the drills before the cutters, the small before the big
    RackAssignment b;
    set_slot(b, false, 3, 1);  // the operator put tool 3 in the first place
    const std::vector<RackTool> more = {{false, 3, 1.0}, {false, 1, 0.6}, {true, 2, 0}, {false, 2, 0.8}};
    fill_in_order(more, b, defined);
    CHECK(slot_of(b, false, 3) == 1 && slot_of(b, false, 1) == 2 && slot_of(b, false, 2) == 3 && slot_of(b, true, 2) == 4);
    // when the places run out the rest stay without one
    RackAssignment c;
    fill_in_order(more, c, {1, 2});
    CHECK(slot_of(c, false, 1) == 1 && slot_of(c, false, 2) == 2 && slot_of(c, false, 3) == 0 && slot_of(c, true, 2) == 0);
    CHECK(selectable_places({14, 0, 3, 3, 15}) == std::vector<int>({3, 14}));
}

static void chaining_the_programs() {
    RackAssignment a;
    set_slot(a, false, 1, 3);
    set_slot(a, false, 2, 1);
    set_slot(a, true, 1, 5);
    ProgramTools drill;
    drill.loaded = true;
    drill.tools = {{false, 2, 0.8}, {false, 1, 0.6}};
    ProgramTools route;
    route.loaded = true;
    route.tools = {{true, 1, 0}};
    // the drilling leaves its last tool in the chuck when a routing is still to come
    RackPlan d = plan_rack(drill, route, false, a);
    CHECK(d.carry_out && !d.carry_in && d.first == 0 && d.slots.size() == 2 && d.slots[0] == std::make_pair(2, 1) && d.slots[1] == std::make_pair(1, 3));
    // no routing: nothing is carried, the places still go
    ProgramTools none;
    RackPlan alone = plan_rack(drill, none, false, a);
    CHECK(!alone.carry_out && alone.present());
    // the routing takes over the last drill (tool 1, in place 3) when the drilling ran chained, and its tools wait after the drills
    drill.done = true;
    drill.chained = true;
    RackPlan r = plan_rack(drill, route, true, a);
    CHECK(r.carry_in && !r.carry_out && r.first == 2 && r.carry_slot == 3 && r.slots.size() == 1 && r.slots[0] == std::make_pair(1, 5));
    // the drilling did not run chained (the server did not plan it so): no carry
    ProgramTools plain = drill;
    plain.chained = false;
    RackPlan q = plan_rack(plain, route, true, a);
    CHECK(!q.carry_in && q.first == 0 && q.carry_slot == 0);

    // the request: the first version has neither field, the second carries the bar
    Json j;
    CHECK(parse_json(job_request("m1", false, "a.ncd", "M48\n", "", {}, 0, nullptr).body, j) && j.get("rack") == nullptr && j.get("request_version") == nullptr);
    const RackPlan empty;
    CHECK(!empty.present() && job_request("m1", false, "a.ncd", "M48\n", "", {}, 0, &empty).body == job_request("m1", false, "a.ncd", "M48\n", "").body);
    CHECK(parse_json(job_request("m1", true, "a.plt", "IN;", "", {}, 0, &r).body, j));
    const Json* rk = j.get("rack");
    CHECK(j.num("request_version") == 2 && rk != nullptr && rk->num("first") == 2 && rk->get("carry_in")->b && !rk->get("carry_out")->b && rk->num("carry_slot") == 3);
    CHECK(rk->get("slots")->a.size() == 1 && rk->get("slots")->a[0].num("tool") == 1 && rk->get("slots")->a[0].num("place") == 5);
    CHECK(parse_rack_applied("{\"id\":\"j\",\"rack_applied\":true}") && !parse_rack_applied("{\"id\":\"j\",\"rack_applied\":false}"));
    CHECK(!parse_rack_applied("{\"id\":\"j\"}") && !parse_rack_applied("not json") && !parse_rack_applied("{\"rack_applied\":\"yes\"}"));
}

// ---- the machine with the tool changer in the workflow ----------------------------------------------------------------------------------

static void drilling_then_routing_on_the_tool_changer() {
    std::string why;
    Json j;
    Workflow w;
    CHECK(w.choose("board.NCD", kDrill, why) && w.choose("outline.PLT", kRoute, why));
    // before the fleet says the model nothing is known about the changer: the request is the one of the first version, and the places are not checked
    CHECK(!w.atc() && w.rack_problems().empty());
    CHECK(parse_json(w.create_request("m1").body, j) && j.get("rack") == nullptr);
    w.on_fleet(fleet_with("", "ccd-atc"), "m1");
    CHECK(w.atc());
    // the places were given in order: the drills small to big, then the pens
    CHECK(w.rack_slot(false, 1) == 1 && w.rack_slot(false, 2) == 2 && w.rack_slot(false, 3) == 3 && w.rack_slot(true, 1) == 4 && w.rack_slot(true, 2) == 5);
    CHECK(w.rack_problems().empty() && w.rack_tools().size() == 5);
    // the drilling request carries the places in the order of use and leaves its last tool in the chuck
    CHECK(parse_json(w.create_request("m1").body, j) && j.num("request_version") == 2);
    const Json* rk = j.get("rack");
    CHECK(rk != nullptr && rk->num("first") == 0 && !rk->get("carry_in")->b && rk->get("carry_out")->b && rk->get("slots")->a.size() == 3);
    CHECK(rk->get("slots")->a[0].num("tool") == 2 && rk->get("slots")->a[0].num("place") == 2 && rk->get("slots")->a[2].num("tool") == 3);
    // the server planned with it
    CHECK(w.on_created(answer(201, "{\"id\":\"d1\",\"rack_applied\":true}"), why) && w.rack_reply() == Workflow::RackReply::Applied);
    CHECK(w.on_started(answer(200, "{}"), why));
    // the squares of the bar while the drilling is at its last tool
    w.on_fleet(fleet_with("d1", "ccd-atc", 3, 4), "m1");
    std::vector<BarTool> bar = w.rack_bar();
    CHECK(bar.size() == 5 && bar[0].tool == 2 && bar[0].state == BarState::Done && bar[1].tool == 1 && bar[1].state == BarState::Done);
    CHECK(bar[2].tool == 3 && bar[2].state == BarState::InUse && bar[2].slot == 3 && bar[3].route && bar[3].state == BarState::Pending);
    // the place of a tool cannot be changed while the job runs
    CHECK(!w.set_rack_place(false, 1, 7) && w.rack_slot(false, 1) == 1);
    w.on_fleet(fleet_with("", "ccd-atc"), "m1");
    CHECK(w.stage() == Stage::Finished && w.want_report());
    CHECK(w.on_report(answer(200, "{\"state\":\"done\",\"holes_done\":4,\"holes_total\":4}")));
    // the drilling is done but the routing comes next: that is a stage, not the end of the job
    CHECK(w.ended_well() && !w.job_complete() && w.stages_done() == 0 && w.next_name() == "outline.PLT");
    CHECK(w.advance() && w.stages_done() == 1 && w.stage_next_name() == "outline.PLT" && w.route());
    // the routing takes over the last drill (tool 3, in place 3) and its pens wait after the three drills
    CHECK(parse_json(w.create_request("m1").body, j));
    rk = j.get("rack");
    CHECK(rk != nullptr && rk->num("first") == 3 && rk->get("carry_in")->b && !rk->get("carry_out")->b && rk->num("carry_slot") == 3);
    CHECK(rk->get("slots")->a.size() == 2 && rk->get("slots")->a[0].num("place") == 4 && rk->get("slots")->a[1].num("place") == 5);
    CHECK(w.on_created(answer(201, "{\"id\":\"r1\",\"rack_applied\":true}"), why) && w.on_started(answer(200, "{}"), why));
    w.on_fleet(fleet_with("r1", "ccd-atc", 1, 2), "m1");
    w.on_fleet(fleet_with("", "ccd-atc"), "m1");
    CHECK(w.on_report(answer(200, "{\"state\":\"done\"}")));
    // now the whole job is over: the board can come out
    CHECK(w.ended_well() && w.job_complete() && w.stages_done() == 1);
}

static void a_server_that_does_not_chain() {
    std::string why;
    Json j;
    Workflow w;
    CHECK(w.choose("board.NCD", kDrill, why) && w.choose("outline.PLT", kRoute, why));
    w.on_fleet(fleet_with("", "ccd-atc"), "m1");
    // the server makes the job but does not say it used the bar: the tools go back to their places after their work, and the screen says so
    CHECK(w.on_created(answer(201, "{\"id\":\"d1\"}"), why) && w.rack_reply() == Workflow::RackReply::NotApplied);
    CHECK(w.on_started(answer(200, "{}"), why));
    w.on_fleet(fleet_with("d1", "ccd-atc"), "m1");
    w.on_fleet(fleet_with("", "ccd-atc"), "m1");
    CHECK(w.on_report(answer(200, "{\"state\":\"done\"}")) && w.advance());
    CHECK(parse_json(w.create_request("m1").body, j));
    const Json* rk = j.get("rack");
    CHECK(rk != nullptr && rk->num("first") == 0 && !rk->get("carry_in")->b && rk->get("slots")->a.size() == 2);
    CHECK(w.rack_reply() == Workflow::RackReply::NotSent);

    // a server that turns the bar down with 400 or 422 gets the job again without it
    Workflow v;
    CHECK(v.choose("board.NCD", kDrill, why));
    v.on_fleet(fleet_with("", "ccd-atc"), "m1");
    CHECK(parse_json(v.create_request("m1").body, j) && j.get("rack") != nullptr);
    CHECK(!v.on_created(answer(422, "{\"error\":\"unknown_field\"}"), why) && v.take_rack_retry() && !v.take_rack_retry());
    CHECK(parse_json(v.create_request("m1").body, j) && j.get("rack") == nullptr && j.get("request_version") == nullptr);
    // an error that is not about the field does not drop it
    Workflow u;
    CHECK(u.choose("board.NCD", kDrill, why));
    u.on_fleet(fleet_with("", "ccd-atc"), "m1");
    CHECK(!u.on_created(answer(500, "{}"), why) && !u.take_rack_retry());
    CHECK(parse_json(u.create_request("m1").body, j) && j.get("rack") != nullptr);
    // the switch of the settings: no bar goes
    std::vector<Problem> pr;
    Settings off = parse_settings("rack_chain = off\n", pr);
    Workflow t;
    t.configure_rack(off);
    CHECK(t.choose("board.NCD", kDrill, why));
    t.on_fleet(fleet_with("", "ccd-atc"), "m1");
    CHECK(parse_json(t.create_request("m1").body, j) && j.get("rack") == nullptr);
    // a machine with the manual change gets none either
    Workflow m;
    CHECK(m.choose("board.NCD", kDrill, why));
    m.on_fleet(fleet_with("", "ccd-mtc"), "m1");
    CHECK(!m.atc() && parse_json(m.create_request("m1").body, j) && j.get("rack") == nullptr && m.rack_problems().empty());
}

static void the_start_waits_for_the_places() {
    std::string why;
    Workflow w;
    CHECK(w.choose("board.NCD", kDrill, why) && w.choose("outline.PLT", kRoute, why));
    w.on_fleet(fleet_with("", "ccd-atc"), "m1");
    CHECK(w.rack_problems().empty());
    // a place that two tools share
    CHECK(w.set_rack_place(false, 1, 2));
    std::vector<RackProblem> p = w.rack_problems();
    CHECK(p.size() == 1 && p[0].kind == RackProblem::Kind::Shared && p[0].tool == 1 && p[0].other_tool == 2 && p[0].slot == 2);
    const std::string text_en = rack_problem_text(p, Lang::En);
    CHECK(has(text_en, "Tool 1 @2") && has(text_en, "shares its place in the bar"));
    CHECK(has(rack_problem_text(p, Lang::Es), "comparte hueco en la barra") && has(rack_problem_text(p, Lang::Pt), "partilha o lugar na barra"));
    // a tool without a place
    CHECK(w.set_rack_place(false, 1, 0) && w.set_rack_place(true, 2, 0));
    p = w.rack_problems();
    CHECK(p.size() == 2 && p[0].kind == RackProblem::Kind::Unassigned && p[1].route && p[1].tool == 2);
    // a place the machine does not define (the setting says which it defines)
    std::vector<Problem> pr;
    Settings six = parse_settings("rack_places = 1-6\n", pr);
    CHECK(pr.empty());
    w.configure_rack(six);
    CHECK(w.rack_places().size() == 6);
    CHECK(w.set_rack_place(false, 1, 9) && w.set_rack_place(true, 2, 6));
    p = w.rack_problems();
    CHECK(p.size() == 1 && p[0].kind == RackProblem::Kind::Undefined && p[0].tool == 1 && p[0].slot == 9);
    CHECK(!w.set_rack_place(false, 77, 3));  // a tool the job does not have
    // a tool put in a free place that the machine defines
    CHECK(w.set_rack_place(false, 1, 1));
    CHECK(w.rack_problems().empty());
    // the check can be turned off (a bench setting): nothing stops the start
    Settings no_check = parse_settings("rack_check = off\n", pr);
    CHECK(w.set_rack_place(false, 1, 9));
    w.configure_rack(no_check);
    CHECK(w.rack_problems().empty());
    CHECK(w.set_rack_place(false, 1, 2) && w.set_rack_place(false, 2, 2) && w.rack_problems().empty());
}

// ---- the stages of a job ----------------------------------------------------------------------------------------------------------------

static void the_end_of_a_stage_is_not_the_end_of_the_job() {
    EndWatcher e;
    CHECK(e.update(0, false) == EndNotice::None);
    CHECK(e.update(1, false) == EndNotice::StageDone);
    CHECK(e.update(1, false) == EndNotice::None && e.update(1, false) == EndNotice::None);
    CHECK(e.update(1, true) == EndNotice::BoardOut);
    CHECK(e.update(1, true) == EndNotice::None);  // said once
    CHECK(e.update(1, false) == EndNotice::None);
    CHECK(e.update(2, false) == EndNotice::StageDone);  // the next job has its own
    CHECK(e.update(2, true) == EndNotice::BoardOut);
    e.reset();
    CHECK(e.update(0, true) == EndNotice::BoardOut);
    CHECK(has(stage_text("outline.PLT", Lang::En), "The drilling is done and the routing comes next") && has(stage_text("outline.PLT", Lang::En), "outline.PLT"));
    CHECK(has(stage_text("", Lang::Es), "fresado") && !has(stage_text("", Lang::Es), "\n"));

    // a job of one program is complete when it ends well
    std::string why;
    Workflow w;
    CHECK(w.choose("board.NCD", kDrill, why) && w.on_created(answer(201, "{\"id\":\"d1\"}"), why) && w.on_started(answer(200, "{}"), why));
    w.on_fleet(fleet_with("d1", "ccd-mtc"), "m1");
    w.on_fleet(fleet_with("", "ccd-mtc"), "m1");
    CHECK(!w.job_complete());  // the server has not said how it ended
    CHECK(w.on_report(answer(200, "{\"state\":\"done\"}")) && w.job_complete() && w.stages_done() == 0);
    // a job that was cancelled is neither a stage nor complete
    Workflow c;
    CHECK(c.choose("board.NCD", kDrill, why) && c.choose("outline.PLT", kRoute, why));
    CHECK(c.on_created(answer(201, "{\"id\":\"d1\"}"), why) && c.on_started(answer(200, "{}"), why));
    c.on_fleet(fleet_with("d1", "ccd-mtc"), "m1");
    c.on_fleet(fleet_with("", "ccd-mtc"), "m1");
    CHECK(c.on_report(answer(200, "{\"state\":\"cancelled\",\"holes_done\":1,\"holes_total\":4}")) && !c.job_complete() && !c.advance() && c.stages_done() == 0);
    // the notice of a stage is its own file
    NoticeSetting voice;
    CHECK(parse_notice("voice:es:f", voice) && notice_file("/sdcard", voice, Cue::Stage) == "/sdcard/voices/es/f/stage.wav");
}

// ---- the card of the work in hand -------------------------------------------------------------------------------------------------------

static void the_card_of_the_work() {
    std::string why;
    Workflow w;
    CHECK(w.choose("board.NCD", kDrill, why) && w.on_created(answer(201, "{\"id\":\"d1\"}"), why) && w.on_started(answer(200, "{}"), why));
    const std::string extra = ",\"estimate_s\":100,\"position\":{\"x\":12.5,\"y\":3,\"z\":4.25},\"feed\":1200,\"spindle_rpm\":24000,\"tool_changes\":2,\"elapsed_s\":75,\"depth_mm\":1.6,\"routed_mm\":0";
    const FleetView f = fleet_with("d1", "ccd-atc", 3, 4, extra);
    w.on_fleet(f, "m1");
    CHECK(f.server_time_s == 1800000000 && f.machines[0].pos_known && close_to(f.machines[0].pos_z, 4.25) && f.machines[0].tool_changes == 2 && close_to(f.machines[0].elapsed_s, 75));
    const JobBar bar = w.bar();
    // the clock of the server is 08:00:00 UTC; with the local time zone one hour ahead and 25 s left the job ends at 09:00
    JobCard c = make_job_card(bar, &f.machines[0], false, f.server_time_s, 60, 3);
    CHECK(c.known && c.tool == 3 && close_to(c.diameter_mm, 1.0) && c.slot == 3 && c.step == 1 && c.steps == 1);
    CHECK(c.left_s == 25 && c.elapsed_s == 75 && c.end_known && c.end_hour == 9 && c.end_minute == 0);
    CHECK(c.pos_known && close_to(c.x, 12.5) && close_to(c.depth_mm, 1.6) && close_to(c.feed, 1200) && close_to(c.rpm, 24000) && c.holes_done == 3 && c.holes_total == 4 && c.changes == 2);
    CardText t = card_text(c, Lang::En);
    CHECK(has(t.block[0], "Tool 3") && has(t.block[0], "1.00 mm") && has(t.block[0], "@3") && has(t.block[0], "1/1"));
    CHECK(has(t.block[1], "Time left 0:25") && has(t.block[1], "Elapsed 1:15") && has(t.block[1], "Ends at 09:00"));
    CHECK(has(t.block[2], "X 12.50  Y 3.00") && has(t.block[2], "Z 4.25  (1.60)") && has(t.block[2], "1200") && has(t.block[2], "24000 rpm"));
    CHECK(has(t.block[3], "3/4") && has(t.block[3], "Tool changes 2"));
    CHECK(has(card_text(c, Lang::Es).block[1], "Transcurrido 1:15") && has(card_text(c, Lang::De).block[1], "Endet um 09:00"));
    // the time zone behind UTC wraps over midnight
    JobCard back = make_job_card(bar, &f.machines[0], false, f.server_time_s, -600, 0);
    CHECK(back.end_hour == 22 && back.end_minute == 0 && back.slot == 0);
    // the slot the fleet says wins over the one the screen gave
    FleetView g = fleet_with("d1", "ccd-atc", 3, 4, ",\"estimate_s\":100,\"tool_slot\":7");
    CHECK(make_job_card(bar, &g.machines[0], false, 0, 0, 3).slot == 7);
    // what the server does not say is a dash, never a number; a clock that is not set gives no end hour
    FleetView plain = fleet_with("d1", "ccd-atc", 3, 4, ",\"estimate_s\":100");
    JobCard d = make_job_card(bar, &plain.machines[0], false, 0, 0, 0);
    CHECK(!d.end_known && !d.pos_known && d.changes == -1 && d.elapsed_s == 75);  // the elapsed time is the estimate less the time left
    CardText td = card_text(d, Lang::En);
    CHECK(has(td.block[1], "Ends at -") && has(td.block[2], "X -  Y -") && has(td.block[2], "Z -") && has(td.block[2], "- rpm") && has(td.block[3], "Tool changes -"));
    // a routing job counts the length cut
    JobCard r = d;
    r.route = true;
    r.routed_mm = 12.34;
    CHECK(has(card_text(r, Lang::En).block[3], "Length routed 12.3 mm"));
    // no job, no card
    CardText none = card_text(make_job_card(JobBar{}, nullptr, false, 0, 0, 0), Lang::En);
    CHECK(none.block[0].empty() && none.block[1].empty() && none.block[2].empty() && none.block[3].empty());
    CHECK(hour_text(7, 5) == "07:05");
}

// ---- the panel that moves the machine by hand -------------------------------------------------------------------------------------------

static Settings jog_settings(const char* text) {
    std::vector<Problem> p;
    Settings s = parse_settings(text, p);
    CHECK(p.empty());
    return s;
}

static void the_jog_panel_moves_one_step() {
    const Settings s = jog_settings("jog_board_z_mm = 20\njog_clearance_mm = 5\n");
    JogPanel jog(s, make_jog_limits(300, 200, s));
    std::vector<JogOrder> o;
    const JogGate free;
    CHECK(jog.steps().size() == 5 && jog.step_index() == 2 && close_to(jog.step_mm(), 1) && !jog.continuous() && jog.speed() == JogSpeed::Medium && close_to(jog.feed(), 1200));
    CHECK(jog.limits().z_floor == 20 && jog.limits().safe_z == 15);
    // one step per press, at the feed of the speed
    CHECK(jog.press(JogDir::XPlus, free, o) == JogRefusal::None && o.size() == 1 && close_to(o[0].dx, 1) && close_to(o[0].feed, 1200) && close_to(jog.pos().x, 1) && jog.touched());
    jog.set_step(0);
    CHECK(close_to(jog.step_mm(), 0.01));
    jog.step_up();
    jog.step_up();
    CHECK(jog.step_index() == 2);
    jog.step_down();
    CHECK(close_to(jog.step_mm(), 0.1));
    jog.set_step(9);  // not a step of the list: nothing changes
    CHECK(jog.step_index() == 1);
    jog.set_speed(JogSpeed::Fast);
    jog.set_step(4);  // 50 mm, cut to the longest single move the settings allow (20 mm)
    CHECK(jog.press(JogDir::YPlus, free, o) == JogRefusal::None && close_to(o[0].dy, 20) && close_to(o[0].feed, 3000));
    // the end of the travel: the move that would pass the end goes to the end, and the next press is refused
    jog.set_speed(JogSpeed::Medium);
    for (int i = 0; i < 8; ++i) jog.press(JogDir::YPlus, free, o);
    CHECK(close_to(jog.pos().y, 180));
    CHECK(jog.press(JogDir::YPlus, free, o) == JogRefusal::None && close_to(o[0].dy, 20) && close_to(jog.pos().y, 200));
    CHECK(jog.press(JogDir::YPlus, free, o) == JogRefusal::Limit && o.empty());
    CHECK(jog.press(JogDir::XMinus, free, o) == JogRefusal::None && close_to(o[0].dx, -1) && close_to(jog.pos().x, 0));  // 20 mm asked from x = 1, cut at the edge
}

static void the_jog_panel_keeps_the_head_clear_of_the_board() {
    const Settings s = jog_settings("jog_board_z_mm = 20\njog_clearance_mm = 5\n");
    JogPanel jog(s, make_jog_limits(300, 200, s));
    std::vector<JogOrder> o;
    const JogGate free;
    jog.set_step(4);  // 20 mm
    // Z down to the surface of the board, no deeper (the tool may not go below it by hand unless the settings allow it)
    CHECK(jog.press(JogDir::ZDown, free, o) == JogRefusal::None && close_to(o[0].dz, 20) && close_to(jog.pos().z, 20));
    CHECK(jog.press(JogDir::ZDown, free, o) == JogRefusal::Limit);
    // too low to cross the board: Z goes up first
    CHECK(jog.press(JogDir::XPlus, free, o) == JogRefusal::RaiseZ && o.empty() && close_to(jog.pos().x, 0));
    // going to a place: up to the clearance, across, then down to its depth
    CHECK(jog.goto_pos(JogPos{100, 50, 20}, free, o) == JogRefusal::None && o.size() == 3);
    CHECK(close_to(o[0].dz, -5) && close_to(o[1].dx, 100) && close_to(o[1].dy, 50) && close_to(o[1].dz, 0) && close_to(o[2].dz, 5) && close_to(jog.pos().z, 20));
    // a place outside the travel is brought inside it
    CHECK(jog.goto_pos(JogPos{900, -4, 30}, free, o) == JogRefusal::None && close_to(jog.pos().x, 300) && close_to(jog.pos().y, 0) && close_to(jog.pos().z, 20));
    // home: up, across, up
    CHECK(jog.home(free, o) == JogRefusal::None && o.size() == 3 && close_to(o[0].dz, -5) && close_to(jog.pos().x, 0) && close_to(jog.pos().z, 0));
    CHECK(jog.home(free, o) == JogRefusal::None && o.empty());  // already there

    // the switch `jog_safe_z` off: the head crosses at any depth
    const Settings loose = jog_settings("jog_board_z_mm = 20\njog_safe_z = off\n");
    JogPanel b(loose, make_jog_limits(300, 200, loose));
    b.set_step(4);
    b.press(JogDir::ZDown, free, o);
    CHECK(b.press(JogDir::XPlus, free, o) == JogRefusal::None && close_to(o[0].dx, 20));
    // the switch `jog_limits` off: the head is not held inside the table
    const Settings open = jog_settings("jog_limits = off\n");
    JogPanel c(open, make_jog_limits(300, 200, open));
    CHECK(c.press(JogDir::XMinus, free, o) == JogRefusal::None && close_to(c.pos().x, -1));
    // with no depth of the board in the settings Z does not move by hand
    const Settings flat = jog_settings("");
    JogPanel z(flat, make_jog_limits(300, 200, flat));
    CHECK(z.press(JogDir::ZDown, free, o) == JogRefusal::Limit);
    // the depth the tool may go below the surface
    const Settings below = jog_settings("jog_board_z_mm = 20\njog_below_board_mm = 0.5\n");
    JogPanel d(below, make_jog_limits(300, 200, below));
    d.set_step(4);
    d.press(JogDir::ZDown, free, o);
    CHECK(d.press(JogDir::ZDown, free, o) == JogRefusal::None && close_to(jog_clamp(d.pos().z, 0, 100), 20.5));
}

static void the_jog_panel_is_blocked_by_a_job_or_the_stop() {
    const Settings s = jog_settings("jog_board_z_mm = 20\n");
    JogPanel jog(s, make_jog_limits(300, 200, s));
    std::vector<JogOrder> o;
    JogGate running;
    running.job_running = true;
    JogGate stop;
    stop.estop = true;
    CHECK(jog.press(JogDir::XPlus, running, o) == JogRefusal::Job && o.empty() && !jog.touched());
    CHECK(jog.press(JogDir::XPlus, stop, o) == JogRefusal::Estop && o.empty());
    CHECK(jog.home(running, o) == JogRefusal::Job && jog.goto_pos(JogPos{1, 1, 0}, stop, o) == JogRefusal::Estop);
    // the switch `jog_block_running` off lets a move go while a job runs; the stop blocks all the same
    const Settings bench = jog_settings("jog_block_running = off\n");
    JogPanel b(bench, make_jog_limits(300, 200, bench));
    CHECK(b.press(JogDir::XPlus, running, o) == JogRefusal::None);
    JogGate both;
    both.job_running = true;
    both.estop = true;
    CHECK(b.press(JogDir::XPlus, both, o) == JogRefusal::Estop);
}

static void a_held_arrow_goes_in_stretches() {
    const Settings s = jog_settings("jog_continuous = on\njog_chunk_mm = 2\njog_board_z_mm = 20\n");
    JogPanel jog(s, make_jog_limits(300, 200, s));
    std::vector<JogOrder> o;
    const JogGate free;
    CHECK(jog.continuous());
    // the press only holds the arrow; the stretches go out at the pace of the speed (2 mm at 1200 mm/min is every tenth of a second)
    CHECK(jog.press(JogDir::XPlus, free, o) == JogRefusal::None && o.empty() && jog.held() && jog.held_dir() == JogDir::XPlus);
    CHECK(jog.tick(0.05, free, o) == JogRefusal::None && o.size() == 1 && close_to(o[0].dx, 2));
    CHECK(jog.tick(0.04, free, o) == JogRefusal::None && o.empty());
    CHECK(jog.tick(0.06, free, o) == JogRefusal::None && o.size() == 1);
    CHECK(jog.tick(0.35, free, o) == JogRefusal::None && o.size() == 4);
    CHECK(close_to(jog.pos().x, 12));
    // let go
    jog.release();
    CHECK(!jog.held() && jog.tick(1, free, o) == JogRefusal::None && o.empty());
    // a long wait is not made up with more than a few stretches
    jog.press(JogDir::YPlus, free, o);
    CHECK(jog.tick(60, free, o) == JogRefusal::None && o.size() == static_cast<std::size_t>(JogPanel::kMaxChunksPerTick));
    // a job or the stop ends the hold
    JogGate stop;
    stop.estop = true;
    CHECK(jog.tick(0.5, stop, o) == JogRefusal::Estop && !jog.held());
    // the end of the travel ends the hold; what was sent before still goes
    jog.park();
    jog.set_step(0);
    CHECK(jog.press(JogDir::XMinus, free, o) == JogRefusal::None && jog.held());
    CHECK(jog.tick(0.5, free, o) == JogRefusal::Limit && !jog.held());
    // turning the mode off lets go of a held arrow
    jog.press(JogDir::YPlus, free, o);
    jog.set_continuous(false);
    CHECK(!jog.held());
    // an arrow that cannot move the head is not held (too low to cross)
    jog.set_continuous(true);
    jog.set_step(4);
    jog.set_continuous(false);
    jog.press(JogDir::ZDown, free, o);
    jog.set_continuous(true);
    CHECK(jog.press(JogDir::XPlus, free, o) == JogRefusal::RaiseZ && !jog.held());
    jog.stop();
}

static void the_four_places_and_the_orders() {
    const Settings s = jog_settings("jog_board_z_mm = 20\n");
    JogPanel jog(s, make_jog_limits(300, 200, s));
    std::vector<JogOrder> o;
    const JogGate free;
    jog.set_step(4);
    jog.press(JogDir::XPlus, free, o);
    jog.press(JogDir::YPlus, free, o);
    CHECK(!jog.has_place(0) && jog.save_place(0) && jog.has_place(0) && close_to(jog.place(0).x, 20) && close_to(jog.place(0).y, 20) && !jog.save_place(4));
    jog.home(free, o);
    CHECK(jog.goto_place(0, free, o) == JogRefusal::None && o.size() == 1 && close_to(o[0].dx, 20) && close_to(jog.pos().y, 20));
    // an empty place does nothing; a job still blocks it
    CHECK(jog.goto_place(2, free, o) == JogRefusal::None && o.empty());
    JogGate running;
    running.job_running = true;
    CHECK(jog.goto_place(2, running, o) == JogRefusal::Job && jog.goto_place(0, running, o) == JogRefusal::Job);

    // the ids the gate shows and the screen reads back
    CHECK(jog_order_id(JogDir::XPlus, 1, false) == "jog X +1 mm" && jog_order_id(JogDir::XMinus, 0.1, false) == "jog X -0.1 mm");
    CHECK(jog_order_id(JogDir::ZUp, 10, false) == "jog Z +10 mm" && jog_order_id(JogDir::ZDown, 50, false) == "jog Z -50 mm" && jog_order_id(JogDir::YMinus, 1, true) == "jog Y -");
    JogDir d = JogDir::XPlus;
    bool step = false;
    CHECK(parse_jog_id("jog Z +10 mm", d, step) && d == JogDir::ZUp && step);
    CHECK(parse_jog_id("jog Y -", d, step) && d == JogDir::YMinus && !step);
    for (const char* bad : {"", "jog", "jog Q +", "jog X x", "jogX +1", "start job"}) CHECK(!parse_jog_id(bad, d, step));
    CHECK(parse_jog_command("jog home").command == JogCommand::Home && jog_home_id() == "jog home");
    CHECK(parse_jog_command("jog goto 3").command == JogCommand::Goto && parse_jog_command("jog goto 3").place == 2 && jog_goto_id(0) == "jog goto 1");
    CHECK(parse_jog_command("jog goto 5").command == JogCommand::None && parse_jog_command("jog goto").command == JogCommand::None);
    CHECK(parse_jog_command("jog X -5 mm").command == JogCommand::Press && parse_jog_command("jog X -5 mm").dir == JogDir::XMinus && parse_jog_command("home").command == JogCommand::None);
    CHECK(order_text(Action{Kind::Jog, "jog X +1 mm"}, Lang::En) == "Jog X +1 mm" && order_text(Action{Kind::Jog, "jog goto 2"}, Lang::En) == "P2");
    CHECK(order_text(Action{Kind::Jog, "jog home"}, Lang::Es) == text(T::OrdHome, Lang::Es));
}

static void the_gate_and_a_move_by_hand() {
    Settings s;
    s.hw_stop_required = true;
    Gate g(s);
    g.hardware_stop(true, false);
    CHECK(g.acknowledge_release());
    const Action jog{Kind::Jog, "jog X +1 mm"};
    const Action other{Kind::Motion, "home"};
    // no job runs: a move by hand asks for the confirmation, any other move needs the arming that a running job gives
    g.link(false);
    Decision d = g.request(jog, Origin::Touch, 1000);
    CHECK(d.verdict == Verdict::NeedConfirm);
    Action out;
    CHECK(g.confirm(Origin::Touch, 1500, out) && out.what == "jog X +1 mm");
    CHECK(g.request(other, Origin::Touch, 1000).why == Why::NotArmed);
    // a job runs: the move by hand is refused, and the refusal says why
    g.link(true);
    d = g.request(jog, Origin::Touch, 1000);
    CHECK(d.verdict == Verdict::Refused && d.why == Why::JobRunning && std::string(why_text(Why::JobRunning, Lang::En)) == "A job is running: no moves by hand");
    CHECK(has(why_text(Why::JobRunning, Lang::Pt), "em curso") && has(why_text(Why::JobRunning, Lang::Ru), "задание"));
    // a confirmation that was waiting when the job started does not go
    g.link(false);
    CHECK(g.request(jog, Origin::Touch, 1000).verdict == Verdict::NeedConfirm);
    g.link(true);
    CHECK(!g.confirm(Origin::Touch, 1100, out));
    // the switch off (a bench setting): the move asks for the confirmation even then
    Settings bench = s;
    bench.jog_block_running = false;
    Gate b(bench);
    b.hardware_stop(true, false);
    b.acknowledge_release();
    b.link(true);
    CHECK(b.request(jog, Origin::Touch, 1000).verdict == Verdict::NeedConfirm);
    // the hardware stop blocks it whatever the switch says
    b.hardware_stop(true, true);
    CHECK(b.request(jog, Origin::Touch, 1000).why == Why::HardwareStop);
    // a spoken order to move by hand still needs the voice switch
    Gate v(s);
    v.hardware_stop(true, false);
    v.acknowledge_release();
    v.link(false);
    CHECK(v.request(jog, Origin::Voice, 1000).why == Why::VoiceOrdersOff);
}

// ---- the settings -----------------------------------------------------------------------------------------------------------------------

static void the_settings_of_the_panel_the_bar_and_the_stages() {
    std::vector<Problem> p;
    const Settings d = parse_settings("", p);
    CHECK(p.empty());
    // every safety default is the conservative one
    CHECK(d.jog_block_running && d.jog_limits && d.jog_safe_z && d.rack_check && d.rack_chain && d.stage_window);
    CHECK(d.jog_steps == std::vector<double>({0.01, 0.1, 1, 10, 50}) && close_to(d.jog_step, 1) && !d.jog_continuous && close_to(d.jog_speed_slow, 300) && close_to(d.jog_speed_medium, 1200) && close_to(d.jog_speed_fast, 3000));
    CHECK(d.jog_speed == JogSpeed::Medium && close_to(d.jog_chunk_mm, 2) && close_to(d.jog_clearance_mm, 5) && close_to(d.jog_board_z_mm, 0) && close_to(d.jog_below_board_mm, 0));
    CHECK(d.rack_places == "1-14" && d.clock_utc_offset_min == 0 && d.sound_stage.kind == NoticeSetting::Kind::Off);

    const Settings s = parse_settings(
        "jog_steps = 0.5, 1, 5\njog_step = 5\njog_continuous = on\njog_speed_slow = 100\njog_speed_medium = 800\njog_speed_fast = 4000\njog_speed = fast\njog_chunk_mm = 1.5\n"
        "jog_clearance_mm = 3\njog_board_z_mm = 18\njog_below_board_mm = 0.5\njog_block_running = off\njog_limits = off\njog_safe_z = off\n"
        "rack_places = 1-6,9\nrack_check = off\nrack_chain = off\nstage_window = off\nclock_utc_offset_min = -120\nsound_stage = voice:es:f\n",
        p);
    CHECK(p.empty());
    CHECK(s.jog_steps == std::vector<double>({0.5, 1, 5}) && close_to(s.jog_step, 5) && s.jog_continuous && close_to(s.jog_speed_slow, 100) && close_to(s.jog_speed_medium, 800) && close_to(s.jog_speed_fast, 4000));
    CHECK(s.jog_speed == JogSpeed::Fast && close_to(s.jog_chunk_mm, 1.5) && close_to(s.jog_clearance_mm, 3) && close_to(s.jog_board_z_mm, 18) && close_to(s.jog_below_board_mm, 0.5));
    CHECK(!s.jog_block_running && !s.jog_limits && !s.jog_safe_z && s.rack_places == "1-6,9" && !s.rack_check && !s.rack_chain && !s.stage_window);
    CHECK(s.clock_utc_offset_min == -120 && s.sound_stage.kind == NoticeSetting::Kind::Voice && s.sound_stage.lang == Lang::Es && s.sound_stage.female);
    CHECK(parse_settings("clock_utc_offset_min = 840\n", p).clock_utc_offset_min == 840 && parse_settings("clock_utc_offset_min = -720\n", p).clock_utc_offset_min == -720);

    // a value that cannot be used is reported and the default stays: a typing mistake never opens a switch
    p.clear();
    const Settings bad = parse_settings(
        "jog_steps = 5, 1\njog_steps = 0\njog_steps = 1,2,3,4,5,6,7,8,9\njog_step = 900\njog_speed = warp\njog_speed_fast = 0\njog_chunk_mm = 0\njog_clearance_mm = -1\njog_board_z_mm = 900\n"
        "jog_below_board_mm = 60\njog_block_running = maybe\njog_limits = 2\nrack_places = 20\nrack_places = x\nrack_check = perhaps\nstage_window = no no\nclock_utc_offset_min = 900\nclock_utc_offset_min = -721\nsound_stage = loud\n",
        p);
    CHECK(p.size() == 19);
    CHECK(bad.jog_steps.size() == 5 && close_to(bad.jog_step, 1) && bad.jog_speed == JogSpeed::Medium && close_to(bad.jog_speed_fast, 3000) && close_to(bad.jog_chunk_mm, 2) && close_to(bad.jog_clearance_mm, 5));
    CHECK(close_to(bad.jog_board_z_mm, 0) && close_to(bad.jog_below_board_mm, 0) && bad.jog_block_running && bad.jog_limits && bad.rack_places == "1-14" && bad.rack_check && bad.stage_window);
    CHECK(bad.clock_utc_offset_min == 0 && bad.sound_stage.kind == NoticeSetting::Kind::Off);
    CHECK(p[0].key == "jog_steps" && p[0].reason == CfgReason::Range && p[4].reason == CfgReason::Choice && p[10].reason == CfgReason::OnOff);
}

int main() {
    places_of_the_bar();
    chaining_the_programs();
    drilling_then_routing_on_the_tool_changer();
    a_server_that_does_not_chain();
    the_start_waits_for_the_places();
    the_end_of_a_stage_is_not_the_end_of_the_job();
    the_card_of_the_work();
    the_jog_panel_moves_one_step();
    the_jog_panel_keeps_the_head_clear_of_the_board();
    the_jog_panel_is_blocked_by_a_job_or_the_stop();
    a_held_arrow_goes_in_stretches();
    the_four_places_and_the_orders();
    the_gate_and_a_move_by_hand();
    the_settings_of_the_panel_the_bar_and_the_stages();
    std::printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
