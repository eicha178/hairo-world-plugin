/**
   \file
   \author Kenta Suzuki
*/

#include "BoxTerrainBuilderDialog.h"
#include <cnoid/Button>
#include <cnoid/FileDialog>
#include <cnoid/LineEdit>
#include <cnoid/MainWindow>
#include <cnoid/MenuManager>
#include <cnoid/ProjectManager>
#include <cnoid/SpinBox>
#include <cnoid/stdx/filesystem>
#include <cnoid/UTF8>
#include <QApplication>
#include <QDebug>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QStyle>
#include <fstream>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "gettext.h"

using namespace std;
using namespace cnoid;
namespace filesystem = cnoid::stdx::filesystem;

BoxTerrainBuilderDialog* stepFieldBuilderDialog = nullptr;

namespace {

struct DialogButtonInfo {
    QDialogButtonBox::ButtonRole role;
    char* label;
};


DialogButtonInfo dialogButtonInfo[] = {
    { QDialogButtonBox::ResetRole,       _("&Reset") },
    { QDialogButtonBox::ActionRole,       _("&Save") },
    { QDialogButtonBox::ActionRole, _("&Save As...") },
    { QDialogButtonBox::ActionRole,       _("&Load") },
    { QDialogButtonBox::AcceptRole,         _("&Ok") }
};

}

namespace cnoid {

class BoxTerrainBuilderDialogImpl
{
public:
    BoxTerrainBuilderDialogImpl(BoxTerrainBuilderDialog* self);
    BoxTerrainBuilderDialog* self;

    DoubleSpinBox* scaleSpin;
    LineEdit* inputFileLine;
    LineEdit* outputFileLine;

    enum DialogButtonId { RESET, SAVE, SAVEAS, LOAD, OK, NUM_DBUTTONS };
    PushButton* dialogButtons[NUM_DBUTTONS];

    void onAccepted();
    void onRejected();
    void onSaveAsButtonClicked();
    void onLoadButtonClicked();
    void onResetButtonClicked();
    void onSaveButtonClicked();
    string getSaveFilename(FileDialog& dialog);
};


class CellManager
{
public:
    CellManager();
    ~CellManager();

    bool read(const string& fileName);
    int getXsize() const;
    int getYsize() const;
    double getPointax(const int& x, const int& y, const int& index) const;
    double getPointay(const int& x, const int& y, const int& index) const;
    double getPointaz(const int& x, const int& y, const int& index) const;
    double getPointbx(const int& x, const int& y, const int& index) const;
    double getPointby(const int& x, const int& y, const int& index) const;
    double getPointbz(const int& x, const int& y, const int& index) const;
    int getId(const int& x, const int& y, const int& index, const int& sindex) const;

private:
    class CellManagerImpl;
    CellManagerImpl *impl;
};

class CellManager::CellManagerImpl
{
public:
    bool read(const string& fileName);

    double height[512][512];
    double cella[512][512][4];
    double cellb[512][512][4];
    double pointa[512][512][4][3];
    double pointb[512][512][4][3];

    double xsize;
    double ysize;
    int id;
};


CellManager::CellManager()
{
    impl = new CellManagerImpl;
}


CellManager::~CellManager()
{
    delete impl;
}


bool CellManager::read(const string& fileName)
{
    impl->read(fileName);
    return true;
}


bool CellManager::CellManagerImpl::read(const string& fileName)
{
    //set height
    ifstream ifs(fileName.c_str());
    if(!ifs) {
        return false;
    }

    string str;
    int row = 0;
    int clm = 0;

    while(getline(ifs, str)) {
        string token;
        istringstream stream(str);
        row = 0;
        while(getline(stream, token, ',')) {
            height[clm][row] = atof(token.c_str());
            if(height[clm][row] < 0.0) {
                height[clm][row] = 0.0;
            }
            row++;
        }
        clm++;
    }
    xsize = row;
    ysize = clm;

    id = 0;

    //set cella
    for(int k = 0; k < ysize; k++) {
        for(int j = 0; j < xsize; j++) {
            for(int i = 0; i < 4; i++) {
                cella[k][j][i] = id++;
            }
        }
    }

    //set cellb
    for(int k = 0; k < ysize; k++) {
        for(int j = 0; j < xsize; j++) {
            for(int i = 0; i < 4; i++) {
                cellb[k][j][i] = id++;
            }
        }
    }

    //set pointa, pointb
    double scale = 0.1;
    if(stepFieldBuilderDialog) {
        scale *= stepFieldBuilderDialog->scale();
    }
    for(int j = 0; j < ysize; j++) {
        for(int i = 0; i < xsize; i++) {
            pointa[j][i][0][0] = scale * (i - xsize / 2);
            pointa[j][i][0][1] = scale * (j - ysize / 2) * -1;
            pointa[j][i][0][2] = scale * height[j][i];
            pointa[j][i][1][0] = scale * (i - xsize / 2);
            pointa[j][i][1][1] = scale * (j + 1 - ysize / 2) * -1;
            pointa[j][i][1][2] = scale * height[j][i];
            pointa[j][i][2][0] = scale * (i + 1 - xsize / 2);
            pointa[j][i][2][1] = scale * (j + 1 - ysize / 2) * -1;
            pointa[j][i][2][2] = scale * height[j][i];
            pointa[j][i][3][0] = scale * (i + 1 - xsize / 2);
            pointa[j][i][3][1] = scale * (j - ysize / 2) * -1;
            pointa[j][i][3][2] = scale * height[j][i];

            pointb[j][i][0][0] = scale * (i - xsize / 2);
            pointb[j][i][0][1] = scale * (j - ysize / 2) * -1;
            pointb[j][i][0][2] = scale * 0.0;
            pointb[j][i][1][0] = scale * (i - xsize / 2);
            pointb[j][i][1][1] = scale * (j + 1 - ysize / 2) * -1;
            pointb[j][i][1][2] = scale * 0.0;
            pointb[j][i][2][0] = scale * (i + 1 - xsize / 2);
            pointb[j][i][2][1] = scale * (j + 1 - ysize / 2) * -1;
            pointb[j][i][2][2] = scale * 0.0;
            pointb[j][i][3][0] = scale * (i + 1 - xsize / 2);
            pointb[j][i][3][1] = scale * (j - ysize / 2) * -1;
            pointb[j][i][3][2] = scale * 0.0;
        }
    }

    return true;
}


int CellManager::getXsize() const
{
    return impl->xsize;
}


int CellManager::getYsize() const
{
    return impl->ysize;
}


double CellManager::getPointax(const int& x, const int& y, const int& index) const
{
    return impl->pointa[y][x][index][0];
}


double CellManager::getPointay(const int& x, const int& y, const int& index) const
{
    return impl->pointa[y][x][index][1];
}


double CellManager::getPointaz(const int& x, const int& y, const int& index) const
{
    return impl->pointa[y][x][index][2];
}


double CellManager::getPointbx(const int& x, const int& y, const int& index) const
{
    return impl->pointb[y][x][index][0];
}


double CellManager::getPointby(const int& x, const int& y, const int& index) const
{
    return impl->pointb[y][x][index][1];
}


double CellManager::getPointbz(const int& x, const int& y, const int& index) const
{
    return impl->pointb[y][x][index][2];
}


int CellManager::getId(const int& x, const int& y, const int& index, const int& sindex) const
{
    if(sindex == 0) {
        return impl->cella[y][x][index];
    } else {
        return impl->cellb[y][x][index];
    }
}

}


BoxTerrainBuilderDialog::BoxTerrainBuilderDialog()
{
    impl = new BoxTerrainBuilderDialogImpl(this);
}


BoxTerrainBuilderDialogImpl::BoxTerrainBuilderDialogImpl(BoxTerrainBuilderDialog* self)
    : self(self)
{
    self->setWindowTitle(_("BoxTerrainBuilder"));
    QVBoxLayout* vbox = new QVBoxLayout();
    QGridLayout* gbox = new QGridLayout();
    int index = 0;

    scaleSpin = new DoubleSpinBox();
    scaleSpin->setDecimals(1);
    scaleSpin->setSingleStep(0.1);
    scaleSpin->setValue(1.0);
    scaleSpin->setRange(0.1, 10.0);
    gbox->addWidget(new QLabel(_("scale[0.1-10.0]")), index, 0);
    gbox->addWidget(scaleSpin, index++, 1);

    inputFileLine = new LineEdit();
    inputFileLine->setEnabled(false);
    gbox->addWidget(new QLabel(_("Input File (.csv)")), index, 0);
    gbox->addWidget(inputFileLine, index++, 1);

    outputFileLine = new LineEdit();
    outputFileLine->setEnabled(false);
    gbox->addWidget(new QLabel(_("Output File (.body)")), index, 0);
    gbox->addWidget(outputFileLine, index++, 1);

    vbox->addLayout(gbox);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(self);
    for(int i = 0; i < NUM_DBUTTONS; ++i) {
        DialogButtonInfo info = dialogButtonInfo[i];
        dialogButtons[i] = new PushButton(info.label);
        PushButton* dialogButton = dialogButtons[i];
        buttonBox->addButton(dialogButton, info.role);
        if(i == OK) {
            dialogButton->setDefault(true);
        }
    }


    self->connect(buttonBox,SIGNAL(accepted()), self, SLOT(accept()));
    vbox->addWidget(buttonBox);

    self->setLayout(vbox);
    dialogButtons[RESET]->sigClicked().connect([&](){ onResetButtonClicked(); });
    dialogButtons[SAVE]->sigClicked().connect([&](){ onSaveButtonClicked(); });
    dialogButtons[SAVEAS]->sigClicked().connect([&](){ onSaveAsButtonClicked(); });
    dialogButtons[LOAD]->sigClicked().connect([&](){ onLoadButtonClicked(); });
}


BoxTerrainBuilderDialog::~BoxTerrainBuilderDialog()
{
    delete impl;
}


void BoxTerrainBuilderDialog::initialzeClass(ExtensionManager* ext)
{
    if(!stepFieldBuilderDialog) {
        stepFieldBuilderDialog = ext->manage(new BoxTerrainBuilderDialog());
    }

    MenuManager& menuManager = ext->menuManager();
    menuManager.setPath("/Tools");
    menuManager.addItem(_("BoxTerrainBuilder"))
            ->sigTriggered().connect([](){ stepFieldBuilderDialog->show(); });
}


double BoxTerrainBuilderDialog::scale() const
{
    return impl->scaleSpin->value();
}


void BoxTerrainBuilderDialog::onAccepted()
{
    impl->onAccepted();
}


void BoxTerrainBuilderDialogImpl::onAccepted()
{

}


void BoxTerrainBuilderDialog::onRejected()
{
    impl->onRejected();
}


void BoxTerrainBuilderDialogImpl::onRejected()
{

}


void BoxTerrainBuilderDialogImpl::onSaveAsButtonClicked()
{
    FileDialog dialog(MainWindow::instance());
    dialog.setWindowTitle(_("Save a Body file"));
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setViewMode(QFileDialog::List);
    dialog.setLabelText(QFileDialog::Accept, _("Save"));
    dialog.setLabelText(QFileDialog::Reject, _("Cancel"));
    dialog.setOption(QFileDialog::DontConfirmOverwrite);

    QStringList filters;
    filters << _("Body files (*.body)");
    filters << _("Any files (*)");
    dialog.setNameFilters(filters);

    dialog.updatePresetDirectories();

    ProjectManager* pm = ProjectManager::instance();
    string currentProjectFile = pm->currentProjectFile();
    filesystem::path path(currentProjectFile);
    string currentProjectName = path.stem().string();
    if(!dialog.selectFilePath(currentProjectFile)) {
        dialog.selectFile(currentProjectName);
    }

    if(dialog.exec() == QDialog::Accepted) {
        QString fileName = QString::fromStdString(getSaveFilename(dialog));
        outputFileLine->setText(fileName);
        onSaveButtonClicked();
    }
}


void BoxTerrainBuilderDialogImpl::onLoadButtonClicked()
{
    FileDialog dialog(MainWindow::instance());
    dialog.setWindowTitle(_("Open a CSV file"));
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setViewMode(QFileDialog::List);
    dialog.setLabelText(QFileDialog::Accept, _("Open"));
    dialog.setLabelText(QFileDialog::Reject, _("Cancel"));

    QStringList filters;
    filters << _("CSV files (*.csv)");
    filters << _("Any files (*)");
    dialog.setNameFilters(filters);

    dialog.updatePresetDirectories();

    if(dialog.exec()) {
        string filename = dialog.selectedFiles().front().toStdString();
        inputFileLine->setText(filename);
    }
}


void BoxTerrainBuilderDialogImpl::onResetButtonClicked()
{
    inputFileLine->clear();
    outputFileLine->clear();
    scaleSpin->setValue(1.0);
}


void BoxTerrainBuilderDialogImpl::onSaveButtonClicked()
{
    QString inputFileName = inputFileLine->text();
    QString outputFileName = outputFileLine->text();
    if(!inputFileName.isEmpty()) {
        if(outputFileName.isEmpty()) {
            onSaveAsButtonClicked();
        } else {
            FILE* fp = fopen(outputFileName.toStdString().c_str(), "w");
            if(fp == NULL) {
                qCritical().noquote() << "cannot open body file." << endl;
            } else {
                CellManager cm;
                if(!cm.read(inputFileName.toStdString())) {
                    qCritical().noquote() << "cannot csv body file." << endl;
                } else {
                    filesystem::path path(outputFileName.toStdString());
                    string bodyName = path.stem().string();

                    fprintf(fp, "format: ChoreonoidBody\n");
                    fprintf(fp, "formatVersion: 1.0\n");
                    fprintf(fp, "name: %s\n", bodyName.c_str());
                    fprintf(fp, "links:\n");
                    fprintf(fp, "  -\n");
                    fprintf(fp, "    name: STEPFIELD\n");
                    fprintf(fp, "#    parent: \n");
                    fprintf(fp, "    translation: [ 0, 0, 0 ]\n");
                    fprintf(fp, "    rotation: [ [ 1, 0, 0, 0 ], [ 0, 1, 0, 0 ], [ 0, 0, 1, 0 ] ]\n");
                    fprintf(fp, "    jointType: fixed\n");
                    fprintf(fp, "    material: Ground\n");
                    fprintf(fp, "    elements:\n");
                    fprintf(fp, "      Shape:\n");
                    fprintf(fp, "        geometry:\n");
                    fprintf(fp, "          type: IndexedFaceSet\n");
                    fprintf(fp, "          coordinate: [\n");


                    for(int k = 0; k < cm.getYsize(); k++) {
                        for(int j = 0; j < cm.getXsize(); j++) {
                            for(int i = 0; i < 4; i++) {
                                fprintf(fp, "            %4.2f, %4.2f, %4.2f,\n", cm.getPointax(j, k, i), cm.getPointay(j, k, i), cm.getPointaz(j, k, i));
                            }
                        }
                    }

                    for(int k = 0; k < cm.getYsize(); k++) {
                        for(int j = 0; j < cm.getXsize(); j++) {
                            for(int i = 0; i < 4; i++) {
                                fprintf(fp, "            %4.2f, %4.2f, %4.2f,\n", cm.getPointbx(j, k, i), cm.getPointby(j, k, i), cm.getPointbz(j, k, i));
                            }
                        }
                    }

                    fprintf(fp, "          ]\n");
                    fprintf(fp, "          coordIndex: [\n");

                    for(int j = 0; j < cm.getYsize(); j++) {
                        for(int i = 0; i < cm.getXsize(); i++) {
                            fprintf(fp,  "            %d, %d, %d, %d, -1,\n", cm.getId(i, j, 0, 0), cm.getId(i, j, 1, 0), cm.getId(i, j, 2, 0), cm.getId(i, j, 3, 0));
                            fprintf(fp,  "            %d, %d, %d, %d, -1,\n", cm.getId(i, j, 0, 1), cm.getId(i, j, 3, 1), cm.getId(i, j, 2, 1), cm.getId(i, j, 1, 1));
                            if(i != 0) {
                                fprintf(fp,  "            %d, %d, %d, %d, -1,\n", cm.getId(i - 1, j, 3, 0), cm.getId(i - 1, j, 2, 0), cm.getId(i, j, 1, 0), cm.getId(i, j, 0, 0));
                            } else {
                                fprintf(fp,  "            %d, %d, %d, %d, -1,\n", cm.getId(i, j, 1, 1), cm.getId(i, j, 1, 0), cm.getId(i, j, 0, 0), cm.getId(i, j, 0, 1));
                            }

                            if(j != 0) {
                                fprintf(fp,  "            %d, %d, %d, %d, -1,\n", cm.getId(i, j - 1, 1, 0), cm.getId(i, j, 0, 0), cm.getId(i, j, 3, 0), cm.getId(i, j - 1, 2, 0));
                            } else {
                                fprintf(fp,  "            %d, %d, %d, %d, -1,\n", cm.getId(i, j, 0, 1), cm.getId(i, j, 0, 0), cm.getId(i, j, 3, 0), cm.getId(i, j, 3, 1));
                            }

                            if(j == cm.getYsize() - 1) {
                                fprintf(fp,  "            %d, %d, %d, %d, -1,\n", cm.getId(i, j, 1, 0), cm.getId(i, j, 1, 1), cm.getId(i, j, 2, 1), cm.getId(i, j, 2, 0));
                            }

                            if(i == cm.getXsize() - 1) {
                                fprintf(fp,  "            %d, %d, %d, %d, -1,\n", cm.getId(i, j, 2, 1), cm.getId(i, j, 3, 1), cm.getId(i, j, 3, 0), cm.getId(i, j, 2, 0));
                            }
                        }
                    }

                    fprintf(fp, "          ]\n");
                    fprintf(fp, "        appearance:\n");
                    fprintf(fp, "          material:\n");
                    fprintf(fp, "            diffuseColor: [ 1, 1, 1 ]\n");
                    fprintf(fp, "#          texture:\n");
                    fprintf(fp, "#            url: \"texture/oak.png\"\n");
                }
            }
            fclose(fp);
        }
    }
}


string BoxTerrainBuilderDialogImpl::getSaveFilename(FileDialog& dialog)
{
    std::string filename;
    auto filenames = dialog.selectedFiles();
    if(!filenames.isEmpty()){
        filename = filenames.front().toStdString();
        filesystem::path path(fromUTF8(filename));
        string ext = path.extension().string();
        if(ext != ".body"){
            filename += ".body";
        }
    }
    return filename;
}
