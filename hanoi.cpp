#include <iostream>
#include <stdexcept>
#include <vector>

using namespace std;

class Tower {
public:
    // конструктор и метод SetDisks нужны, чтобы правильно создать башни
    Tower(int disks_num) {
        FillTower(disks_num);
    }

    void PrintDisks() {
        for (auto i : disks_) {
            cout << ' ' << i;
        }
        cout << endl;
    }

    int GetDisksNum() const {
        return disks_.size();
    }

    void SetDisks(int disks_num) {
        FillTower(disks_num);
    }

    void RemoveDisks() {
        disks_.clear();
    }

    // добавляем диск на верх собственной башни
    // обратите внимание на исключение, которое выбрасывается этим методом
    void AddToTop(int disk) {
        int top_disk_num = disks_.size() - 1;
        if (0 != disks_.size() && disk >= disks_[top_disk_num]) {
            throw invalid_argument("Невозможно поместить большой диск на маленький");
        }
        else {
            // допишите этот метод и используйте его в вашем решении
            disks_.push_back(disk);
        }
    }

    // вы можете дописывать необходимые для вашего решения методы

    int RemoveFromTop() {
        int top_disk_num = disks_.size() - 1;
        if (0 == disks_.size()) {
            throw invalid_argument("Невозможно снять диск с пустой башни");
        }
        else {
            int i = disks_[top_disk_num];
            disks_.pop_back();
            return i;
        }
    }


    // disks_num - количество перемещаемых дисков
    // destination - конечная башня для перемещения
    // buffer - башня, которую нужно использовать в качестве буфера для дисков
    void MoveDisks(int disks_num, Tower& destination, Tower& buffer) {
        if (disks_num > 0) {
            //--disks_num;
            
            MoveDisks(disks_num - 1, buffer, destination);
            cout << '@' << disks_num << "@\n";
            destination.AddToTop(disks_num);
            MoveDisks(disks_num - 1, destination, buffer);
        }/*
        else if (disks_num == 3) {
            destination.AddToTop(1);
            buffer.AddToTop(2);
            buffer.AddToTop(destination.RemoveFromTop());
            destination.AddToTop(3);
            buffer.RemoveFromTop();
            destination.AddToTop(buffer.RemoveFromTop());
            destination.AddToTop(1);
        }
        else if (disks_num == 2) {
            buffer.AddToTop(1);
            destination.AddToTop(2);
            destination.AddToTop(buffer.RemoveFromTop());
        }*/
        else {
            return;
        }


    }

private:
    vector<int> disks_;

    // используем приватный метод FillTower, чтобы избежать дубликации кода
    void FillTower(int disks_num) {
        for (int i = disks_num; i > 0; i--) {
            disks_.push_back(i);
        }
    }
};


 void SolveHanoi(vector<Tower>& towers) {
     int disks_num = towers[0].GetDisksNum();
         // запускаем рекурсию
         // просим переложить все диски на последнюю башню
         // с использованием средней башни как буфера
         towers[0].MoveDisks(disks_num, towers[2], towers[1]);
         towers[0].RemoveDisks();
 }

 int main() {
     int towers_num = 3;
     int disks_num = 3;
     vector<Tower> towers;
     // добавим в вектор три пустые башни
     for (int i = 0; i < towers_num; ++i) {
         towers.push_back(0);
     }
     // добавим на первую башню три кольца
     towers[0].SetDisks(disks_num);
     SolveHanoi(towers);

     cout << "Tower 1:";
     towers[0].PrintDisks();
     cout << "Tower 2:";
     towers[1].PrintDisks();
     cout << "Tower 3:";
     towers[2].PrintDisks();
 }
