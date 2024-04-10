#include <iostream>
#include <string>  

bool sum(){
    int sum=0;
    while(1){
        sum++;
        if(sum==5){
            return false;
        }
        std::cout<<sum<<std::endl;
    }
    std::cout<<sum<<std::endl;
    return 1;
}

int main(){
    // char** ch;
    // char* ch1="12345";
    // char* ch2="66";
    // char* ch3="22";
    // ch=&ch1;
    // ch[1]=ch2;
    // ch[2]=ch3;
    //     printf("%s\n",ch1);
    //     printf("%s\n",ch[0]);
    //     printf("%s\n",ch[2]);
    //     int num = std::stod(ch[1]);  // 简单的转换，但没有错误检查
    //     printf("%d\n",num);

    std::cout<<sum()<<std::endl;

    return 0;
}