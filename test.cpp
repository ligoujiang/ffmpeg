#include <iostream>
using namespace std;
int main(){
    int *arry=new int[5];
    arry[4]=6;
    for(int i=0;i<5;i++){
        cout<<arry[i]<<endl;
    }
    return 0;
}