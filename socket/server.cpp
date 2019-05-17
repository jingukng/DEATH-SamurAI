

#include "server.hpp"
namespace asio = boost::asio;
using asio::ip::tcp;
using namespace std;

const int BUFFER_SIZE = 256;

int main(){

    //---ソケット通信用：コネクション確立---//    
    struct sockaddr_in addr;
    int sock;
    char buf[BUFFER_SIZE];
    int data;
    int addr_size;
    string ipaddr;
    int portnum;

    //親プロセスからポート番号(文字列)を受け取る
    cin>>portnum;

    //debug
    // cout << portnum << endl;

    //ソケットプログラミング：ソケット生成
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock<0){
        perror("ERROR opening socket");
        cout << "ERROR opening socket" << endl;
        exit(1);
    }
    addr.sin_family = AF_INET;
    addr.sin_port = htons(portnum);
    addr.sin_addr.s_addr = INADDR_ANY;

    //ソケットプログラミング：ソケット登録
    bind(sock,(struct sockaddr*)&addr, sizeof(addr));

    //ソケットプログラミング：ソケット接続準備
    listen(sock,1);

    //ソケットプログラミング：ソケット接続待機
    addr_size = sizeof(addr);
    sock = accept(sock,(struct sockaddr*)&addr, (socklen_t*)&addr_size);
    //--- コネクション確立 end -----------------------//

    //ソケットプログラミング：送受信0：名前の受信
    memset(buf,0,sizeof(buf));
    data = read(sock,buf,sizeof(buf));     
    cout << buf << endl;  

    //親プロセスからコース情報を取得
    string all;
    cin >> course;

    //コース情報を送信用に整形
    all = to_string(course.thinkTime)+"\n"+
          to_string(course.stepLimit)+"\n"+
          to_string(course.width)+' '+to_string(course.length)+"\n"+
          to_string(course.vision)+"\n";

    //ソケットプログラミング：送受信1：初期化時の入力を送信
    sendto(sock, all.c_str(), all.size()+1,
           0,(struct sockaddr*)&addr,sizeof(addr));
    
    //ソケットプログラミング：送受信2：受信の返事を受信
    memset(buf,0,sizeof(buf));
    data = read(sock,buf,sizeof(buf));

    //親プロセスへ受信結果を出力
    cout << buf << endl;


    //レース情報用のインスタンスを用意
    RaceInfo info;    
    //infoのメンバsquaresの動的メモリ確保
    info.squares = new char*[course.length];
    for(int y=0; y<course.length; y++){
        info.squares[y] = new char[course.width];
    }

    for (int stepNumber = 0;
        stepNumber != 
        course.stepLimit;
        //DEBUG_STEPLIMIT;
        stepNumber++) {

        //親プロセスからコース情報を取得
        cin >> info;

        //親プロセスからのレース情報を送信用に整形
        setRaceInfoForSend(info,all,course.length,course.width);

        //ソケットプログラミング：送受信A：レース情報の送信
        sendto(sock, all.c_str(), all.size()+1,
            0,(struct sockaddr*)&addr,sizeof(addr));
        
        //ソケットプログラミング：送受信B：手(加速度)の受信
        memset(buf,0,sizeof(buf));
        data = read(sock,buf,sizeof(buf));

        //親プロセスに受信した手を出力
        cout << buf << endl;
    }

    //ソケットプログラミング：送受信3：ソケット切断
    close(sock);
}


//ステップ毎のレース情報を送信用に整形する
void setRaceInfoForSend(RaceInfo &info, string &all, int length, int width){
    all = to_string(info.stepNumber)+"\n"+
          to_string(info.timeLeft)+"\n"+
          to_string(info.me.position.x)+' '+
          to_string(info.me.position.y)+' '+
          to_string(info.me.velocity.x)+' '+
          to_string(info.me.velocity.y)+"\n"+
          to_string(info.opponent.position.x)+' '+
          to_string(info.opponent.position.y)+' '+
          to_string(info.opponent.velocity.x)+' '+
          to_string(info.opponent.velocity.y)+"\n";
    for(int i=0; i<length; i++){
        for(int j=0; j<width; j++){
            all = (j == width-1 ? 
                   all + to_string(info.squares[i][j]) + "\n":
                   all + to_string(info.squares[i][j]) + ' ' );
        }
    }
}
