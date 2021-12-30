#define CROW_MAIN
#include "include/crow_all.h"
#include "include/inja.hpp"
#include "include/Session.hpp"

#include <iostream>

int main()
{
    // Inherit everything from CookieParser and Session classes
    crow::App<crow::CookieParser, app::middlewares::Session> app;

    CROW_ROUTE(app, "/")([&](){
        
        inja::json data;
        data["test_variable"] = 1;
        return inja::render("Your variable is: {{ test_variable }}", data);
    });
    
    CROW_ROUTE(app, "/set/")([](){

        return "Variable set";
    });

    CROW_ROUTE(app, "/change/")([&](const crow::request& req){
        auto session = app.get_context<app::middlewares::Session>(req).session; // Gets a shared pointer to the user's session
        auto test_result = session->has("test"); // Why does this dump core? It should just invoke a session method and store its result.
        
        return "Variable changed";
    });
    
    app.port(18080).multithreaded().run();
    return 0;
}
