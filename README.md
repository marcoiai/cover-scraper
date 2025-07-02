This program uses https://id.twitch.tv/oauth2/token to search and download
cover art based an a roms directory with roms

You need to create a Twitch account, and get a Client Secret and Client ID

Dependencies:
- Ubuntu asahi: sudo apt install libcurl4-openssl-dev libcjson-dev
- macOS: brew install curl cjson

✅ To compile it on Ubuntu Asahi:
gcc cover-scraper.c -o cover-scraper -I/usr/local/include -L/usr/local/lib -lSDL3 -lSDL3_image -lSDL3_ttf -lcurl -lcjson  
gcc cover-scraper2.c -o cover-scraper -I/usr/local/include -L/usr/local/lib -lSDL3 -lSDL3_image -lSDL3_ttf -lcurl -lcjson  

✅ If you installed libcurl and cJSON via Homebrew:  
clang cover-scraper.c -o cover-scraper -I/usr/local/include -L/usr/local/lib -lSDL3 -lSDL3_image -lSDL3_ttf -lcurl -lcjson  

✅ If you're on an Apple Silicon Mac with /opt/homebrew:  
clang cover-scraper.c -o cover-scraper -I/opt/homebrew/include -L/opt/homebrew/lib -lSDL3 -lSDL3_image -lSDL3_ttf -lcurl -lcjson  

✅ If you're using Frameworks for SDL3 but libcurl and cJSON from Homebrew:  
clang cover-scraper.c -o cover-scraper -F/Library/Frameworks -framework SDL3 -framework SDL3_image -framework SDL3_ttf -I/opt/homebrew/include -L/opt/homebrew/lib -lcurl -lcjson  

⚒️ Check install locations:  
brew --prefix curl  
brew --prefix cjson  

