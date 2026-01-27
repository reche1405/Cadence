#include "project.h"
#include "./ui_project.h"

Project::Project(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::Project)
{
    ui->setupUi(this);
}

Project::~Project()
{
    delete ui;
}
