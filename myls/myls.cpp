#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <utime.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>

const int BUF_SIZE = 1024;
char path_buf[BUF_SIZE]; //用于存储路径的缓冲区
char workDir[2] = "."; //默认工作目录为当前目录

int output_info(struct stat stat)
{
    // 初始化
    char mode[11] = "----------";
    // 使用宏判断文件类型
    if(S_ISREG(stat.st_mode)) mode[0] = '-'; //普通文件
    else if(S_ISDIR(stat.st_mode)) mode[0] = 'd'; //目录文件
    else if(S_ISCHR(stat.st_mode)) mode[0] = 'c'; //字符特殊文件
    else if(S_ISBLK(stat.st_mode)) mode[0] = 'b'; //块特殊文件
    else if(S_ISFIFO(stat.st_mode)) mode[0] = 'p'; //FIFO文件
    else if(S_ISSOCK(stat.st_mode))mode[0] = 's'; //套接口文件
    else if(S_ISLNK(stat.st_mode)) mode[0] = 'l'; //符号链接
    // 通过与运算判断文件权限
    if(stat.st_mode & S_IRUSR) mode[1] = 'r'; //文件所有者是否可读
    if(stat.st_mode & S_IWUSR) mode[2] = 'w'; //文件所有者是否可写
    if(stat.st_mode & S_IXUSR) mode[3] = 'x'; //文件所有者是否可执行
    if(stat.st_mode & S_IRGRP) mode[4] = 'r'; //组用户是否可读
    if(stat.st_mode & S_IWGRP) mode[5] = 'w'; //组用户是否可写
    if(stat.st_mode & S_IXGRP) mode[6] = 'x'; //组用户是否可执行
    if(stat.st_mode & S_IROTH) mode[7] = 'r'; //其他用户是否可读
    if(stat.st_mode & S_IWOTH) mode[8] = 'w'; //其他用户是否可写
    if(stat.st_mode & S_IXOTH) mode[9] = 'x'; //其他用户是否可执行
    printf("%s ", mode);

    struct passwd *p_passwd;
    struct group *p_group;

    p_passwd = getpwuid(stat.st_uid);
    p_group = getgrgid(stat.st_gid);
    
    char time_buf[30];
    struct tm tm_info;
    localtime_r(&stat.st_mtime, &tm_info);
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", &tm_info);

    printf("%d ",(int)stat.st_nlink); //输出硬链接数
    
    if(p_passwd != NULL) printf("%s ", p_passwd->pw_name); //输出用户名
    else printf("%d ",(int)stat.st_uid);
    
    if(p_group != NULL) printf("%s ", p_group->gr_name); //输出组名
    else printf("%d ",stat.st_gid);
    
    printf("%5d ",(int)stat.st_size); //输出文件大小
    
    printf("%s", time_buf); //输出最近修改时间

    return 0;
}

int list_files(const char* path)
{
    DIR *dir = NULL;
    struct dirent *dirEnt = NULL;
    
    dir = opendir(path);
    if(dir == NULL) //目录打开失败输出提示信息
    {
        printf("Fail to open path: \"%s\"\n", path);
        return -1;
    }
    
    while((dirEnt = readdir(dir)) != NULL) //输出所有当前目录下的文件名
    {
        if(dirEnt->d_name[0] == '.') continue;
        printf("%s ",dirEnt->d_name);
    }
    printf("\n");
    closedir(dir);
    return 0;
}

int list_files_long(const char* path)
{
    DIR *dir = NULL;
    struct dirent *dirEnt = NULL;
    struct stat stat;

    memset(path_buf, 0, BUF_SIZE); 
    snprintf(path_buf, BUF_SIZE, "%s", path);
    dir = opendir(path_buf); //打开目录
    if(dir == NULL) //目录打开失败输出提示信息
    {
        printf("Fail to open path: \"%s\"\n", path_buf);
        return -1;
    }
    //输出当前目录下每个文件的信息
    while((dirEnt = readdir(dir)) != NULL) //使用readdir函数逐一读出目录项
    {
        if(dirEnt->d_name[0] == '.') continue;
        snprintf(path_buf, BUF_SIZE, "%s/%s", path, dirEnt->d_name);
        int res = lstat(path_buf, &stat); // 通过lstat函数获取文件信息
        if(res == -1)
        {
            printf("Stat Error: %s\n", path_buf);
            continue;
        }
        output_info(stat);
        printf(" ");
        printf("%s\n", dirEnt->d_name);
    }
    closedir(dir); //关闭目录
    return 0;
}

/*
    getopt对于可选参数不能直接使用空格分隔，只支持“ls -lsomefolder”。
    为了和原版ls尽可能接近，即“ls -l somefolder”的形式，此处做了一些特殊处理：
    选项l不直接接受参数，而是将下一个非选项参数作为路径（如果没有下一个非选项参数，则使用当前路径）。
*/
int main(int argc, char* argv[])
{
    int op;
    bool l_flg = false;
    const char* path = workDir; //默认打开当前工作目录

    while ((op = getopt(argc, argv, "l")) != -1) //解析命令行选项
    {
        switch (op) {
        case 'l': //启用长格式输出
            l_flg = true;
            break;
        default:
            break;
        }
    }
    
    if (optind < argc) strcat(workDir, "/"), strcat(workDir, argv[optind]); // 如果有额外的非选项参数，获取下一个非选项参数（路径名）

    if (l_flg) list_files_long(path); // 如果有 -l 选项，调用长格式函数
    else list_files(path); // 否则调用普通列表函数
    
    return 0;
}