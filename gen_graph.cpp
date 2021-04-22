

    // generating a tree in a simple way
#include <bits/stdc++.h>
using namespace std;

int rand(int a, int b) {
    return a + rand() % (b - a + 1);
}

int main(int argc, char* argv[]) {
    srand(atoi(argv[1]));
    //int n = rand(2, 5);
    int n = rand(6, 50);
    int m = rand(n-1,(n*(n-1))/2);
    printf("%d %d 1\n", n , m);
    set<pair<int,int>> edges;
    for(int i = 2; i <= n; ++i) {
        int u = rand(1,i-1);
        int v = i;
        printf("%d %d\n", u,v);
        edges.insert({u,v});
    }
    while(edges.size()<m){
        
        int x = rand(1,n);
        int y = rand(1,n);
        if(x==y) continue;
        if((edges.find({x,y})!=edges.end()) || (edges.find({y,x})!=edges.end())) continue;
        edges.insert({x,y});
        cout<<x<<" "<<y<<endl;
    }
}