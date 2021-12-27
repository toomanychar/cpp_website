#define CROW_MAIN
#include "include/crow_all.h"
#include "include/inja.hpp"
#include "include/Session.hpp"

#include <iostream>

int main()
{
    // Initialize app with the Session.hpp class as one of the middlewares
    crow::App<crow::CookieParser, app::middlewares::Session> app;
    
    CROW_ROUTE(app, "/")([](){
        inja::json data;
        data["test_variable"] = 1; // How to get the variable from current session?
        return inja::render("{{ test_variable }}", data);
    });
    
    CROW_ROUTE(app, "/test/").methods("GET"_method, "POST"_method)([](const crow::request& req){
        // How to write a variable to current session?
        return "Hello world";
    });

    app.port(18080).multithreaded().run();
    return 0;
}