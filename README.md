# Ollama-summarizer
An entirely c-based website summarizer using ollama models.
##KEY NOTES## 
1_You need to download 3 libraries for this code to run properly. namely libcurl4, libxml2 and libjson-c.  
2_This code can be easily changed to whatever ollama model you have installed on your device. It has a MODEL macro right at the top which is fully configurable.  
3_In linux you can run this to download every necessary libraries :  
```bash
sudo apt install libcurl4-openssl-dev libxml2-dev libjson-c-dev libzip-dev
```
Additionally, I recommend downloading pkgconf for easier compilation :
```bash
sudo apt install pkgconf
```
4_Compilation code in linux :
```bash
gcc -o output ollama.c $(pkg-config --cflags libcurl libxml-2.0 json-c) $(pkg-config --libs libcurl libxml-2.0 json-c)
```
This creates an output file, "output", which you can run with :
```bash
./output
```
