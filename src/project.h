#ifndef PROJECT_H
#define PROJECT_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui {

class Project;
}
QT_END_NAMESPACE

class Project : public QMainWindow
{
    Q_OBJECT

public:
    Project(QWidget *parent = nullptr);
    ~Project();

private:
    Ui::Project *ui;
};
#endif // PROJECT_H
