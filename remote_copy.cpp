//Usage: ./a.out [-r][-json][-src_usrpwd][-dst_usrpwd] -src src_file -dst dst_file

#include<iostream>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<curl/curl.h>
#include<sys/stat.h>
#include<dirent.h>
using namespace std;

#define FILE_URI_PREFIX "file://"
#define SMB_URI_PREFIX "smb://"
#define FTP_URI_PREFIX "ftp://"
#define MAX_LENTH 4096
#define EXP_TIME_OUT 60
#define SMB_FILE_NOT_FOUND_EXIT_CODE 78
#define SMB_COULD_NOT_CONNECT 7
#define UNIQUE_STR_TO_STOP_POLLING "12345678900987654321"

time_t prev_ts = 0;
double prev_dlnow = 0;
double prev_ulnow = 0;
char remotebuf[MAX_LENTH] = "/0";
bool json_output=false;

void show_usage();

class Dict{
    
    char key[30][MAX_LENTH], value[30][MAX_LENTH];
    int dict_lent = -1;

    public:

        Dict(){
            return;
        }

        void parse_cmd_args(int argc, const char *argv[]){
            for(int i=1;i<argc;i++){
                
                if(strcmp(argv[i],"-r")==0){
                    //for recursive copy
                    strcpy(key[++dict_lent], "recursive_copy\0");
                    strcpy(value[dict_lent], "true");
                    
                }

                else if(strcmp(argv[i],"-json")==0){
                    //output type json
                    strcpy(key[++dict_lent], "json_output\0");
                    strcpy(value[dict_lent], "true");
                }
                else if((strcmp(argv[i],"-h")==0)|| (strcmp(argv[i],"--help")==0)){
                    
                    show_usage();
                }
                
                else if(strcmp(argv[i],"-src")==0){
                    strcpy(key[++dict_lent], "src_file\0");
                    
                    if((strncmp(argv[i+1], FILE_URI_PREFIX, 7)==0) || (strncmp(argv[i+1], SMB_URI_PREFIX, 6)==0) || (strncmp(argv[i+1], FTP_URI_PREFIX, 6)==0)){
                        strcpy(value[dict_lent], argv[i+1]);
                    }
                    else
                    {
                        show_usage();
                    }
                }
                
                else if(strcmp(argv[i],"-dst")==0){
                    strcpy(key[++dict_lent], "dst_file\0");
    
                    if((strncmp(argv[i+1], FILE_URI_PREFIX, 7)==0) || (strncmp(argv[i+1], SMB_URI_PREFIX, 6)==0) || (strncmp(argv[i+1], FTP_URI_PREFIX, 6)==0)){
                        strcpy(value[dict_lent], argv[i+1]);
                    }
                    else
                    {
                        show_usage();
                    }
                }

                else if(strcmp(argv[i],"-src_usrpwd")==0){
                    
                    strcpy(key[++dict_lent], "src_usrpwd\0");
                    strcpy(value[dict_lent], argv[i+1]);
                }

                else if(strcmp(argv[i],"-dst_usrpwd")==0){

                    strcpy(key[++dict_lent], "dst_usrpwd\0");
                    strcpy(value[dict_lent], argv[i+1]);
                }


            }

        }

        void print_dict(){
            cout<<"printing args dictionary\n\n";
            for (int i=0;i<=dict_lent;i++){
                cout<<key[i]<<"--->"<<value[i]<<"\n";
            }
            cout<<"************************\n\n";
        }

        bool has_key(const char *search_key){
            for(int i=0; i<=dict_lent; i++){
                if(strcmp(key[i], search_key)==0){
                    return true;
                }
            }
            return false;
        }

        char* get_value(const char *key_){
            for(int i=0; i<=dict_lent; i++){
                if(strcmp(key[i], key_)==0){
                    return value[i];
                }
            }
            throw("Key not found");
        }
        
}options;


bool is_valid_to_copy(Dict *dict){
    if((dict->has_key("src_file")) && (dict->has_key("dst_file"))){
        if(strncmp(dict->get_value("src_file"),SMB_URI_PREFIX, 6)==0){
            if(!dict->has_key("src_usrpwd"))
                return false;
        }
        if(strncmp(dict->get_value("dst_file"),SMB_URI_PREFIX, 6)==0){
            if(!dict->has_key("dst_usrpwd"))
                return false;
        }
        return true;
    }
    return false;
}

off_t fsize(const char *filename)
{
    struct stat64 st;
    if (!stat64(filename, &st))
        return st.st_size;
    return -1;
}

int check_ftp_file_exists(CURL *curl, const char *url, char *usrpwd);

bool is_dir_ftp(CURL *curl, const char *ftp_url, char *usrpwd){
    
    if(check_ftp_file_exists(curl, ftp_url, usrpwd) == 0){
        curl_easy_setopt(curl, CURLOPT_URL, ftp_url);
        
        if(usrpwd != NULL){
            curl_easy_setopt(curl, CURLOPT_USERPWD, usrpwd);
        }
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
        CURLcode res = curl_easy_perform(curl);
        if(res == CURLE_OK)
            return true;
        else
            return false;
    }
    else{
        return false;
    }
}


bool is_dir(const char *filepath){

    struct stat s;
    if( stat(filepath, &s) == 0 ){
        if( s.st_mode & S_IFDIR )
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    else{
        return false;
    }
}

void create_dirs(std::string str){
    str = str.substr(0, str.find_last_of("/\\"));
    if( !is_dir(str.c_str())){
        create_dirs(str);
    }
    else{
      return;
    }
    const int dir_err = mkdir(str.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if (-1 == dir_err)
    {
        cout<<"Error creating directory\n";
        exit(-1);
    }
}

void show_usage(){
    cout<<"\n\n Usage: rcp [-r][-json][-src_usrpwd][-dst_usrpwd] -src src_file -dst dst_file\n";
    cout<<"\n-src     => Specify source file support protocols ftp(ftp://username:password@ip/path), smb(smb://ip/sharename), file(file:///path/to/file) \n";
    cout<<"\n-dst     => Specify dst file support protocols ftp, smb, file \n";
    cout<<"\n-r       => This option is used to for recursive copy Note:: Only SRC_FILE type should be FILE type\n";
    cout<<"\n-json    => Output will be of type json\n";
    cout<<"\n-h       => for help\n";
    cout<<"\n-src_usrpwd\n-dst_usrpwd => These fields  are provided if src or dst is of protocol smb usage: -src_usrpwd username:password \n";
    cout<<"\n\n\n Example: 1)file-file::> rcp -src file:///home/lordkrs/in/karthik.mp4 -dst file:///home/lordkrs/out/karthik.mp4\n";
    cout<<"          2)file-ftp::> rcp -src file:///home/lordkrs/in/karthik.mp4 -dst ftp://karthavya:karthavya@192.168.43.221/karthik.mp4\n";
    cout<<"          3)file-smb::> rcp -src file:///home/lordkrs/in/karthik.mp4 -dst smb://192.168.43.221/temp/karthik.mp4 -dst_usrpwd karthavya:karthavya\n";
    cout<<"          4)ftp-smb::> rcp -src ftp://karthavya:karthavya@192.168.43.221/karthik.mp4 -dst smb://192.168.43.221/temp/karthik.mp4 -dst_usrpwd karthavya:karthavya\n";
    cout<<"          5)recursive copy::>rcp -r -src file:///home/lordkrs/in -dst ftp://karthavya:karthavya@192.168.43.221/\n";
    exit(-1);
}

double byte_to_mb(double byte){
    return byte/(1024*1024);
}

int progress_callback(void *clientp, double dltotal, double dlnow, double ultotal, double ulnow)
{

    double progress = -1;
    struct timespec   ts;
    struct timeval    tp;
    double bandwith ; 

    if (ultotal != 0) {
        progress = ulnow * 100.0 / ultotal;
        bandwith = ulnow - prev_ulnow;
    }
    else if (dltotal != 0) {
        progress = dlnow * 100.0 / dltotal;
        bandwith = dlnow - prev_dlnow;
    }
    gettimeofday(&tp, NULL);

    /* Convert from timeval to timespec */
    ts.tv_sec  = tp.tv_sec;
    ts.tv_nsec = tp.tv_usec * 1000;
    ts.tv_sec += EXP_TIME_OUT;
    
    if ( (time(NULL) - prev_ts) > 1) {
        bandwith = bandwith /(time(NULL)  - prev_ts);

        prev_ts = time(NULL);
        if (ultotal != 0){
            prev_ulnow = ulnow ;
        }else if (dltotal != 0) {
            prev_dlnow = dlnow ;
        }
    }


    if(json_output){
        cout<<"\nNot yet supported";
        exit(-1);
    }
    else{
        if(progress <= 100.0) {
            int barWidth = 70;

            cout << "[";
            int pos = barWidth * progress/100;
            for (int i = 0; i < barWidth; ++i) {
                if (i < pos) cout << "=";
                else if (i == pos) cout << ">";
                else cout << " ";
            }
            cout << "] " << int(progress) <<"%     "<<byte_to_mb(bandwith)<<" Mb/s"<< " \r";
            cout.flush();

        }
    }

    return 0;
}



int check_ftp_file_exists(CURL *curl, const char *path_url, char *usrpwd) {
    int found = -1;
    CURLcode res;

    // set path to check
    curl_easy_setopt(curl, CURLOPT_URL, path_url);
    if(usrpwd != NULL){
        curl_easy_setopt(curl, CURLOPT_USERPWD, usrpwd);
    }
    // download file with no body
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);

    res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
        double filesize = -1;
        curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &filesize);
        found = (filesize != -1.0) ? 0 : 1;
    }
    else {
            cout<<"\nftp connection failed--> error_desc-->"<<curl_easy_strerror(res)<<"\n";
    }

    curl_easy_reset(curl);

    return found;
}

static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream)
{
  size_t written = fwrite(ptr, size, nmemb, (FILE *)stream);
  return written;
}


int lcopy_(const char *src, const char *dst, CURL *curl_handle, char *src_usrpwd, char *dst_usrpwd){
    /*performs local to local and remote to local
    Destination is local
    */
    char errbuf[MAX_LENTH];
    char dst_uri[MAX_LENTH];
    int status = -1;

    strcpy(dst_uri, dst+7);

    if(strncmp(src,FILE_URI_PREFIX,7)==0){
        if (0 != access(src + 7, F_OK)){
            cout<<"Source path not found \n";
            return -1;
        }
        if (fsize(src + 7) <= 0){
            cout<<"Invalid source file given file_size zero \n ";
            return -1;
        }
    }
    else if(strncmp(src,FTP_URI_PREFIX,6)==0)  
    {   

        if (0 != check_ftp_file_exists(curl_handle, src, src_usrpwd)) {
            cout<<"\nSource path not found \n";
            return -1;
        }
    }

    if(strncmp(dst,FILE_URI_PREFIX,7)==0){
        if (0 == access(dst + 7, F_OK)){
            cout<<"Destination path already exists \n";
            return -1;
        }
        create_dirs(dst+7);
    }


    FILE *dest_file_desc = fopen(dst + 7, "w");
    if(dest_file_desc == NULL){
        cout<<"Unable to open dst_file";
        return -1;
    }


    curl_easy_setopt(curl_handle, CURLOPT_URL, src);
    if(src_usrpwd != NULL){
        curl_easy_setopt(curl_handle, CURLOPT_USERPWD, src_usrpwd);
    }
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)dest_file_desc);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl_handle, CURLOPT_ERRORBUFFER, errbuf);
    curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl_handle, CURLOPT_PROGRESSFUNCTION, progress_callback);
    curl_easy_setopt(curl_handle, CURLOPT_PROGRESSDATA, NULL);
    
    int res = curl_easy_perform(curl_handle);

    if (!res) {
        //cout<<"\ncopy completed successfully\n";
        status = 0;
        cout<<"\n";
    }
    else {

        status = -1;
        cout<<"\nError while copying data err_desc::";
        if(strncmp(src,SMB_URI_PREFIX,6)==0){
            if(res == SMB_FILE_NOT_FOUND_EXIT_CODE){
                cout<<"Source file not found\n";
            }
            else if (res == SMB_COULD_NOT_CONNECT)
            {
                cout<<"Could not connect to server\n";
            }
            
        }
    }

    fflush(dest_file_desc);
    fclose(dest_file_desc);


    if(status == -1){
        if(remove(dst)!= 0){
            cout<<"Error while deleting dst file\n";
        }

    }

    return status;
}



int rcopy_(const char *src, const char *dst, CURL *curl_handle, char *src_usrpwd, char *dst_usrpwd){
    //local to remote

    curl_off_t src_fsize;
    CURLcode res;
    int status = -1;

    if(strncmp(src,FILE_URI_PREFIX,7)==0){
        
        if (0 != access(src + 7, F_OK)){
            cout<<"Source path not found \n";
            return -1;
        }

        src_fsize = fsize(src+7);
        if (src_fsize <= 0){
            cout<<"Invalid source file given file_size zero \n ";
            return -1;
        }
    }
    else{
        return -1;
    }

    if(strncmp(dst,FTP_URI_PREFIX,6)==0)  
    {   
        if (0 == check_ftp_file_exists(curl_handle, dst, dst_usrpwd)) {
            cout<<"\nDestination file already exists \n";
            return -1;
        }
    }

    FILE *src_file_desc = fopen(src + 7, "rb");
    if(src_file_desc == NULL){
        cout<<"Unable to open src_file";
        return -1;
    }

    curl_easy_setopt(curl_handle, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl_handle, CURLOPT_URL, dst);
    if(dst_usrpwd != NULL){
        curl_easy_setopt(curl_handle, CURLOPT_USERPWD, dst_usrpwd);
    }
    curl_easy_setopt(curl_handle, CURLOPT_READDATA, src_file_desc);
    curl_easy_setopt(curl_handle, CURLOPT_INFILESIZE_LARGE, (curl_off_t) src_fsize); //src_file_size to upload

    curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl_handle, CURLOPT_PROGRESSFUNCTION, progress_callback);
    curl_easy_setopt(curl_handle, CURLOPT_PROGRESSDATA, NULL);
    if(strncmp(dst,FTP_URI_PREFIX,6)==0)  
    {
        curl_easy_setopt(curl_handle, CURLOPT_FTP_CREATE_MISSING_DIRS, 1L);
    }

    res = curl_easy_perform(curl_handle);
    if (!res) {
        //cout<<"\ncopy completed successfully\n";
        cout<<"\n";
        status = 0;
    }
    else {

        status = -1;
        cout<<"\nError while copying data err_desc::";
        if(strncmp(dst,SMB_URI_PREFIX,6)==0){
            if(res == SMB_FILE_NOT_FOUND_EXIT_CODE){
                cout<<"Dst file not found\n";
            }
            else if (res == SMB_COULD_NOT_CONNECT)
            {
                cout<<"Could not connect to server\n";
            }
            
        }
    }

    fclose(src_file_desc); // close the local file

    return status;

    
}


void upload(){    
    char *src = options.get_value("src_file");
    char *dst = options.get_value("dst_file");
    char *src_usrpwd=NULL, *dst_usrpwd=NULL;
    int status = -1;
    curl_off_t src_fsize;
    
    CURL *upload_curl_handle;
    curl_global_init(CURL_GLOBAL_ALL);
    upload_curl_handle = curl_easy_init();

    if (upload_curl_handle == NULL) {
        cout<<"failed to initialize curl\n";
        exit(-1);
    }

    if(options.has_key("src_usrpwd")){
        src_usrpwd = options.get_value("src_usrpwd");
    } 

    if(options.has_key("dst_usrpwd")){
        dst_usrpwd = options.get_value("dst_usrpwd");
    }

    FILE *src_file_desc = fopen("tmp", "rb");
    if(src_file_desc == NULL){
        cout<<"Unable to open src_file";
        return;
    }
    src_fsize = fsize("tmp");

    curl_easy_setopt(upload_curl_handle, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(upload_curl_handle, CURLOPT_URL, dst);
    if(dst_usrpwd != NULL){
        curl_easy_setopt(upload_curl_handle, CURLOPT_USERPWD, dst_usrpwd);
    }
    curl_easy_setopt(upload_curl_handle, CURLOPT_READDATA, src_file_desc);
    curl_easy_setopt(upload_curl_handle, CURLOPT_INFILESIZE_LARGE, (curl_off_t) src_fsize);
    curl_easy_setopt(upload_curl_handle, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(upload_curl_handle, CURLOPT_PROGRESSFUNCTION, progress_callback);
    curl_easy_setopt(upload_curl_handle, CURLOPT_PROGRESSDATA, NULL);
    if(strncmp(dst,FTP_URI_PREFIX,6)==0)  
    {
        curl_easy_setopt(upload_curl_handle, CURLOPT_FTP_CREATE_MISSING_DIRS, 1L);
    }

    int res = curl_easy_perform(upload_curl_handle);
    if (!res) {
        cout<<"\nupload completed successfully\n";
        status = 0;
    }
    else {

    }

    if(remove("tmp")!= 0){
            cout<<"Error while deleting tmp file\n";
    }
    return ;
    
}



void download(){
    char *src = options.get_value("src_file");
    char *dst = options.get_value("dst_file");
    char *src_usrpwd=NULL, *dst_usrpwd=NULL;
    
    CURL *download_curl_handle;
    curl_global_init(CURL_GLOBAL_ALL);
    download_curl_handle = curl_easy_init();

    if (download_curl_handle == NULL) {
        cout<<"failed to initialize curl\n";
        exit(-1);
    }

    if(options.has_key("src_usrpwd")){
        src_usrpwd = options.get_value("src_usrpwd");
    } 

    if(options.has_key("dst_usrpwd")){
        dst_usrpwd = options.get_value("dst_usrpwd");
    }

    FILE *dest_file_desc = fopen("tmp", "w");
    if(dest_file_desc == NULL){
        cout<<"Unable to open tmp";
        return ;
    }

    char errbuf[MAX_LENTH];
    int status = -1;
    char *rdata = remotebuf;

    curl_easy_setopt(download_curl_handle, CURLOPT_URL, src);
    if(src_usrpwd != NULL){
        curl_easy_setopt(download_curl_handle, CURLOPT_USERPWD, src_usrpwd);
    }
    curl_easy_setopt(download_curl_handle, CURLOPT_WRITEDATA, dest_file_desc);
    curl_easy_setopt(download_curl_handle, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(download_curl_handle, CURLOPT_ERRORBUFFER, errbuf);
    curl_easy_setopt(download_curl_handle, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(download_curl_handle, CURLOPT_PROGRESSFUNCTION, progress_callback);
    curl_easy_setopt(download_curl_handle, CURLOPT_PROGRESSDATA, NULL);
    
    int res = curl_easy_perform(download_curl_handle);

    if (!res) {
        cout<<"\ndowload completed successfully\n";
        status = 0;

    }
    else {
    }
}



int rrcopy_(const char *src, const char *dst, CURL *curl_handle, char *src_usrpwd, char *dst_usrpwd){

    if(strncmp(src,FTP_URI_PREFIX,6)==0)  
    {   

        if (0 != check_ftp_file_exists(curl_handle, src, src_usrpwd)) {
            cout<<"\nSource path not found \n";
            return -1;
        }
    }
    
    if(strncmp(dst,FTP_URI_PREFIX,6)==0)  
    {   

        if (0 == check_ftp_file_exists(curl_handle, dst, dst_usrpwd)) {
            cout<<"\ndst already exists \n";
            return -1;
        }
    }

    download();
    upload();
    return -1;

}


void copy_(const char *src, const char *dst, CURL *curl_handle, char *src_usrpwd, char *dst_usrpwd){
    int res;
    
    cout<<"\nCOPY STARTED from "<<src<<" to "<<dst<<"\n\n\n";
    if(strncmp(dst,FILE_URI_PREFIX,7)==0){
        res = lcopy_(src, dst, curl_handle, src_usrpwd, dst_usrpwd);
    }
    else if(strncmp(src,FILE_URI_PREFIX,7)==0){
        res = rcopy_(src, dst, curl_handle, src_usrpwd, dst_usrpwd);
    }
    else {
        strcpy(remotebuf,"\0");
        res = rrcopy_(src, dst, curl_handle, src_usrpwd, dst_usrpwd);
    }
    
}

int list_files( const char *path, char dir_list[][MAX_LENTH]){
	DIR *dp;
	struct dirent *dirp;
    int i = 0;

	if ((dp = opendir(path))== NULL){
		cout<<"Can't open "<<path;
		return -1;
	}

    
	while((dirp = readdir(dp)) != NULL)
        if((strcmp(dirp->d_name,".")!=0) &&  (strcmp(dirp->d_name,"..")!=0))
            strcpy(dir_list[i++] , dirp->d_name);
	closedir(dp);
    return i;
}

void recursive_copy(const char *src, const char *dst, CURL *curl_handle, char *src_usrpwd, char *dst_usrpwd){
    int res;
    int total_files = 0;
    char dir_list[256][MAX_LENTH], temp_src[MAX_LENTH],temp_dst[MAX_LENTH];
    

    if(strncmp(src, FILE_URI_PREFIX,7)==0){
        total_files = list_files(src+7, dir_list);
    }
    else if (strncmp(src, FTP_URI_PREFIX,6)==0)
    {
        return;
    }
    


    for(int i=0;i<total_files; i++){
        strcpy(temp_src,"\0");
        strcat(temp_src,src);
        strcat(temp_src, "/");
        strcat(temp_src, dir_list[i]);
        

        strcpy(temp_dst,"\0");
        strcat(temp_dst,dst);
        strcat(temp_dst, "/");
        strcat(temp_dst, dir_list[i]);

        if(is_dir(temp_src+7)){
            
            recursive_copy(temp_src, temp_dst, curl_handle, src_usrpwd, dst_usrpwd);
        }
        else{
            //cout<<temp_src<<"\n"<<temp_dst;
            copy_(temp_src, temp_dst, curl_handle, src_usrpwd, dst_usrpwd);
        }
        cout<<"\n\n";
    }
    
    
}

int main(int argc, const char *argv[]){
    options.parse_cmd_args(argc, argv);
    //options.print_dict();
    
    if(!(is_valid_to_copy(&options))){
        show_usage();
    }
    
    CURL *curl_handle;
    curl_global_init(CURL_GLOBAL_ALL);
    curl_handle = curl_easy_init();

    if (curl_handle == NULL) {
        cout<<"failed to initialize curl\n";
        return -1;
    }

    json_output = options.has_key("json_output");
    char *src = options.get_value("src_file");
    char *dst = options.get_value("dst_file");
    char *src_usrpwd=NULL, *dst_usrpwd=NULL;
        
    if(options.has_key("src_usrpwd")){
        src_usrpwd = options.get_value("src_usrpwd");
    } 

    if(options.has_key("dst_usrpwd")){
        dst_usrpwd = options.get_value("dst_usrpwd");
    }
        
    if(options.has_key("recursive_copy")){
        
        if((strncmp(src,FILE_URI_PREFIX,7)==0) &&(strncmp(dst,FILE_URI_PREFIX,7)==0)){
            if(!((is_dir(src+7)) && (is_dir(dst+7)) )){
                cout<<"\n source and destination should be directory\n";
                exit(-1);
            }
        }
        
        recursive_copy(src, dst, curl_handle, src_usrpwd, dst_usrpwd);
    }
    else{
        copy_(src, dst, curl_handle, src_usrpwd, dst_usrpwd);
    }

    if (curl_handle)
        curl_easy_cleanup(curl_handle);
    curl_global_cleanup();

}
