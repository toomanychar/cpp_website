#define CROW_MAIN
#include "include/crow_all.h"
#include "include/inja.hpp"

#include "include/Session.hpp"

#include <iostream>


int main()
{
    // Inherit everything from CookieParser and Session classes
    crow::App<crow::CookieParser, app::middlewares::Session> app;

    CROW_ROUTE(app, "/")([&](const crow::request& req){
        inja::json data;
        data["test_variable"] = 1;
        return inja::render("Your variable is: {{ test_variable }}", data);
    });
    
    CROW_ROUTE(app, "/set/")([&](const crow::request& req){
        
        auto session = app.get_context<app::middlewares::Session>(req).session; // Gets a shared pointer to the user's session
        session->has("Test"); // Dumps core. Why?
        session->test(); // Dumps core. Why?

        return "Variable set";
    });

    CROW_ROUTE(app, "/change/")([&](const crow::request& req){

        return "Variable changed";
    });
    
    app.port(18080).multithreaded().run();
    return 0;
}
