#pragma once

#include <functional>
#include <unordered_map>
#include <string>
#include <mutex>
#include <sstream>
#include <locale>
#include <cstdlib>
#include <algorithm>
#include <utility>
#include <random>

#include "crow_all.h"
#include <boost/format.hpp>
#include <boost/date_time.hpp>

namespace app{
    namespace middlewares{
        
        using boost::format;
        using namespace boost::gregorian;
        
        class Session{
            public:
                using Cookie = crow::CookieParser;
                using request = crow::request;
                using response = crow::response;
                
                static constexpr unsigned SESSION_DURATION_DAYS = 14;
                static constexpr unsigned SID_LENGTH = 128;
                
                static const std::string COOKIE_KEY;
                static const std::string RND_ALPHABET;
                static const char* COOKIE_DATE_FORMAT;
                
            protected:
            	class UserSession{
            		public:
            			using Key = std::string;
            			using Value = crow::json::wvalue;
            			using State = crow::json::wvalue;

            		protected:
            			State state;
            			
            		public:
            			UserSession() = default;
            			UserSession(const UserSession&) = default; //because crow::json::wvalue is not copyable
            			UserSession(UserSession&&) = default;

            			int test_var;
                        void test() {
                            auto test_var_pointer = &(this->test_var); // Doesn't dump[ core]
                            auto test_var_value = this->test_var; // Dumps core
                            this->test_var = 0; // Dumps core
                        }
            			
            			bool has(const Key& key){ return this->state.count(key) > 0; }
            			Value& get(const Key& key){ return this->state[key]; }
            			
            			template <class T>
            			UserSession& set(const Key& key, const T& value){
            				this->state[key] = /*std::forward<T&&>(*/value/*)*/;
            				return *this;
            			}
            			
            			UserSession& remove(const Key& key){
            				this->state[key] = nullptr;
            				return *this;
            			}
            	};
            
                class SessionRepo{
                	public:
                		using Key = std::string;
                		using Value = std::shared_ptr<UserSession>;
                		using map = std::unordered_map<Key, Value>;
                		
                		SessionRepo() = default;
                		SessionRepo(const SessionRepo&) = default; //Value non copyable
                		SessionRepo(SessionRepo&&) = default;
                		
                	protected:
                		map sessions = map{};
                		
            		public:
            			bool has(const Key& key) const{
                            auto it = this->sessions.find(key);
                            return it != this->sessions.end();
                        }
                        
                        const Value& get(const Key& key) const{
                            return this->sessions.at(key);
                        }
                        
                        Value& get(const Key& key){
                            return this->sessions.at(key);
                        }
                        
                        SessionRepo& set(Key key, Value&& value){
                            this->sessions.emplace(std::move(key), std::forward<Value&&>(value));
                            return *this;
                        }
                        
                        SessionRepo& remove(const Key& key){
                        	this->sessions.erase(key);
                        	return *this;
                        }
                        
                        const Value& operator[](const Key& key) const{
                            return this->get(key);
                        }
                        
                        Value& operator[](const Key& key){
                            return this->get(key);
                        }
                };
                
            public:  
            	static SessionRepo sessions;
                static std::mutex sessions_mutex;
                          
                struct context{
                    std::shared_ptr<UserSession> session = nullptr;
                };
                
                /*template<class... Middlewares>
                void init(crow::Crow<Middlewares...>& app){}*/
                
                template <class Cookies>
                bool hasSession(Cookies& cookies) const{
                    return !cookies.get_cookie(COOKIE_KEY).empty();
                }
                
                std::string craftExpiresDate(){                    
                    std::stringstream ss;
                    date expiration = day_clock::local_day() + days(SESSION_DURATION_DAYS);
                    ss.imbue(std::locale(ss.getloc(), new date_facet(COOKIE_DATE_FORMAT))); //cleaning taken care of by the locale
                    ss << expiration;
                    return ss.str();
                }
                
                std::string craftSecureCookieString(const std::string id){
		            #ifdef CROW_ENABLE_SSL
		            	static const char* fmt = "%1%; Expires=%2%; Secure; HttpOnly;";
		            #else
		            	static const char* fmt = "%1%; Expires=%2%; HttpOnly;";
		            #endif
		            
                    return str(
                        format(fmt)
                        % id
                        % this->craftExpiresDate()
                    );
                }
                
                std::string randomStr(unsigned length){
                	static std::random_device rnd;
                	static std::mt19937 gen(rnd());
                	
                    std::string source;
                    for(unsigned i = 0, amount = length/RND_ALPHABET.size() + 1 ; i < amount ; ++i)
                    	source += RND_ALPHABET;
                    
                    std::shuffle(source.begin(), source.end(), gen);
                    return source.substr(0, length);
                }
                
                template <class Cookies>
                std::string makeNewSession(request& rq, Cookies& cookies, context& ctx){
                    std::string sid;
                    do{
                        sid = this->randomStr(SID_LENGTH);
                    }while(sessions.has(sid));
                    
                    CROW_LOG_DEBUG << "Adding new session w/ sid: " << sid;
                    std::shared_ptr<UserSession> session{};
                    ctx.session = session;
                    
                    std::lock_guard<std::mutex> _(sessions_mutex);
                    sessions.set(sid, std::move(session));
                    
                    return this->craftSecureCookieString(sid);
                }
                
                std::string stripCookie(const std::string& cookie){
                	std::istringstream ss{cookie};
                	std::string ret;
                	std::getline(ss, ret, ';');
                	return ret;
                }
                
                template <class Cookies>
                std::string getSessionCookie(const Cookies& cookies){
                	return cookies.get_cookie(COOKIE_KEY);
                }
                
                bool isValidSessionCookie(const std::string& cookie){
                	return sessions.has(stripCookie(cookie));
                }
                
                template <class Contexts>
                void before_handle(request& rq, response& res, context& ctx, Contexts& contexts){
                    auto& cookies = contexts.template get<Cookie>();
                    
                    auto createCookie = [&]{
		                auto cookie = this->makeNewSession(rq, cookies, ctx);
		                cookies.set_cookie(COOKIE_KEY, cookie);
		                return cookie;
                    };
                    
                    auto sessionCookie = stripCookie([&]{
                    	if(!this->hasSession(cookies))
                    		return createCookie();
                    		
                    	return [&]{
                    		auto cookieTmp = this->getSessionCookie(cookies);
				            if(!this->isValidSessionCookie(cookieTmp))
				            	return createCookie();
				            	
				            return cookieTmp;
                    	}();
                    }());
                    
                    ctx.session = sessions.get(sessionCookie);
                }
                
                template <class Contexts>
                void after_handle(request&, response&, context&, Contexts&){}
        };
        
        
        const std::string Session::COOKIE_KEY = "SESS_ID";
        const std::string Session::RND_ALPHABET = "0123456789ABCDEFGHIJKLMNOPQRSTUVXYZabcdefghijlmnpqrstuvwxyz";
        const char* Session::COOKIE_DATE_FORMAT = "%a, %d %b %Y %H:%M:%S %z";
        
        
    	Session::SessionRepo Session::sessions;
        std::mutex Session::sessions_mutex;
    }
}
