#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <curl/curl.h>
#include <libxml/HTMLparser.h>
#include <libxml/xpath.h>
#include <json-c/json.h>

#define MODEL "gpt-oss:120b-cloud"

extern int errno;

size_t write_to_file(void *ptr, size_t itemsize, size_t nitems, void *stream);

int libcurl();

int json_parse();

char *read_prompt(const char *filename);

char *escape_json_string(const char *input);

int main() {
    char temp[1000];
    printf("Gimme your url right here: ");
    if (scanf("%s", temp) != 1) {
        fprintf(stderr, "Error: Failed to read input.\n");
        return -1;
    }
    errno = 0;
    char *url = malloc(strlen(temp) + 1);
    if (!url) {
        fprintf(stderr, "Error: %s\n", strerror(errno));
        return -1;
    }
    strcpy(url, temp);
    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Error: failed to initilize curl.\n");
        curl_easy_cleanup(curl);
        free(url);
        return -1;
    }
    //options
    errno = 0;
    FILE *fp1 = fopen("output1.html", "wb");
    if (!fp1) {
        fprintf(stderr, "Error: %s\n", strerror(errno));
        curl_easy_cleanup(curl);
        free(url);
        return -1;
    }
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0");
    curl_easy_setopt (curl, CURLOPT_URL, url);
    curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, write_to_file);
    curl_easy_setopt (curl, CURLOPT_WRITEDATA, fp1);
    //action
    CURLcode curl_result = curl_easy_perform(curl);
    errno = 0;
    if (fclose(fp1) == EOF) {
        perror("Error: ");
        printf("\n");
        curl_easy_cleanup(curl);
        free(url);
        return -1;
    }
    free(url);
    if (curl_result != CURLE_OK) {
        fprintf(stderr, "download problem: %s\n", curl_easy_strerror(curl_result));
        curl_easy_cleanup(curl);
        return -1;
    }
    //libcurl and xpath parsing the html file
    if (libcurl() != 0) {
        curl_easy_cleanup(curl);
        return -1;
    }
    curl_easy_reset(curl);
    //asking ollama to summerize
    char *prompt = read_prompt("output2.txt");
    if (!prompt) {
        fprintf(stderr, "Error: char *prompt failed.\n");
        curl_easy_cleanup(curl);
        free(prompt);
        return -1;
    }
    char *escaped = escape_json_string(prompt);
    if (!escaped) {
        fprintf(stderr, "Error: Problem in escaping the json response.\n");
        curl_easy_cleanup(curl);
        free(prompt);
        return -1;
    }
    errno = 0;
    char *payload = malloc(strlen(escaped) + 100);
    if (!payload) {
        fprintf(stderr, "Error: %s\n", strerror(errno));
        curl_easy_cleanup(curl);
        free(prompt);
        return -1;
    }
    sprintf(payload, "{\"model\":\"%s\",\"prompt\":\"summerize this: %s\",\"stream\":false}", MODEL, escaped);
    curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:11434/api/generate");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
    errno = 0;
    FILE *fp2 = fopen("output3.json", "wb");
    if (!fp2) {
        fprintf(stderr, "Error: %s\n", strerror(errno));
        curl_easy_cleanup(curl);
        free(prompt);
        free(payload);
        return -1;
    }
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_file);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp2);
    //action
    curl_result = curl_easy_perform(curl);
    if (curl_result != CURLE_OK) {
        fprintf(stderr, "\nOllama request failed: %s\n", curl_easy_strerror(curl_result));
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        fclose(fp2);
        free(prompt);
        free(payload);
        return -1;
    }
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    free(prompt);
    free(payload);
    errno = 0;
    if (fclose(fp2) == EOF) {
        perror("Error: ");
        printf("\n");
        return -1;
    }
    if (json_parse() != 0) {
        return -1;
    }
    return 0;
}

size_t write_to_file(void *ptr, size_t itemsize, size_t nitems, void *stream) {
    return fwrite(ptr, itemsize, nitems, (FILE *) stream);
}

int libcurl() {
    htmlDocPtr doc = htmlReadFile("output1.html", "UTF-8",
                                  HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING | HTML_PARSE_RECOVER);
    if (!doc) {
        fprintf(stderr, "Error: Failed to parse HTML document\n");
        return -1;
    }
    xmlXPathContextPtr context = xmlXPathNewContext(doc);
    if (!context) {
        fprintf(stderr, "Error: Failed to create XPath context\n");
        xmlFreeDoc(doc);
        return -1;
    }
    xmlXPathObjectPtr xpath_result = xmlXPathEvalExpression((const xmlChar *) "//p | //code", context);
    if (!xpath_result) {
        fprintf(stderr, "Error: XPath query failed\n");
        xmlFreeDoc(doc);
        xmlXPathFreeContext(context);
        return -1;
    }
    errno = 0;
    FILE *fp = fopen("output2.txt", "w");
    if (!fp) {
        fprintf(stderr, "Error: %s\n", strerror(errno));
        xmlFreeDoc(doc);
        xmlXPathFreeContext(context);
        xmlXPathFreeObject(xpath_result);
        return -1;
    }
    if (xpath_result->nodesetval) {
        int size = xpath_result->nodesetval->nodeNr;
        for (int i = 0; i < size; ++i) {
            xmlNodePtr node = xpath_result->nodesetval->nodeTab[i];
            xmlChar *content = xmlNodeGetContent(node);
            if (content) {
                fprintf(fp, "%s\n", content);
                xmlFree(content);
            }
        }
    }
    xmlFreeDoc(doc);
    xmlXPathFreeContext(context);
    xmlXPathFreeObject(xpath_result);
    errno = 0;
    if (fclose(fp) == EOF) {
        perror("Error: ");
        printf("\n");
        return -1;
    }
    return 0;
}

int json_parse() {
    errno = 0;
    FILE *fp = fopen("output3.json", "r");
    if (!fp) {
        perror("Error: ");
        printf("\n");
        return -1;
    }

    char *line = NULL;
    size_t len = 0;

    while (getline(&line, &len, fp) != -1) {
        struct json_object *parsed = json_tokener_parse(line);
        if (!parsed) {
            fprintf(stderr, "Failed to parse JSON: %s\n", line);
            free(line);
            fclose(fp);
            return -1;
        }

        struct json_object *response;
        if (json_object_object_get_ex(parsed, "response", &response)) {
            printf("%s", json_object_get_string(response));
        }

        json_object_put(parsed);
    }
    printf("\n");
    free(line);
    errno = 0;
    if (fclose(fp) == EOF) {
        perror("Error: ");
        printf("\n");
        return -1;
    }
    return 0;
}

char *read_prompt(const char *filename) {
    errno = 0;
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "Error: %s\n", strerror(errno));
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    size_t length = ftell(fp);
    rewind(fp);

    errno = 0;
    char *buffer = malloc(length + 1);
    if (!buffer) {
        fprintf(stderr, "Error: %s\n", strerror(errno));
        if (fclose(fp) == EOF) {
            perror("Error: ");
            printf("\n");
            return NULL;
        }
        return NULL;
    }

    size_t read = fread(buffer, 1, length, fp);
    if (read != length) {
        fprintf(stderr, "Error: Failed to read expected data from file.\n");
        fclose(fp);
        return NULL;
    }
    buffer[length] = '\0';
    errno = 0;
    if (fclose(fp) == EOF) {
        perror("Error: ");
        printf("\n");
        return NULL;
    }
    return buffer;
}

char *escape_json_string(const char *input) {
    size_t len = strlen(input);
    size_t out_size = len * 2 + 1;
    errno = 0;
    char *output = malloc(out_size);
    if (!output) {
        fprintf(stderr, "Error: %s\n", strerror(errno));
        return NULL;
    }
    char *p = output;
    for (size_t i = 0; i < len; ++i) {
        switch (input[i]) {
            case '\"':
                *p++ = '\\';
                *p++ = '\"';
                break;
            case '\\':
                *p++ = '\\';
                *p++ = '\\';
                break;
            case '\b':
                *p++ = '\\';
                *p++ = 'b';
                break;
            case '\f':
                *p++ = '\\';
                *p++ = 'f';
                break;
            case '\n':
                *p++ = '\\';
                *p++ = 'n';
                break;
            case '\r':
                *p++ = '\\';
                *p++ = 'r';
                break;
            case '\t':
                *p++ = '\\';
                *p++ = 't';
                break;
            default:
                *p++ = input[i];
                break;
        }
    }
    *p = '\0';
    return output;
}
