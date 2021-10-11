/*
 * DiffMorpher
 * Copyright 2021 The DiffMorpher Author
 * https://github.com/kjoba/diffmorpher
 *
 * Use of this source code is governed by an
 * MIT-style license that can be found in the
 * LICENSE-file or at
 *
 * https://opensource.org/licenses/MIT
 */

#include <QCoreApplication>
#include <QtCore>
#include <QDebug>
#include <QDir>
#include "diff_match_patch.h"

static int handleFiles(QFileInfo sourceFile, QFileInfo targetFile, QFileInfo patchFile, QFileInfo outFile, bool autoOption, bool forceOption, bool ignoreOption, QString fill) {
    //check files
    QFile sourceF(sourceFile.absoluteFilePath());
    QByteArray sourceData;
    if (sourceFile.isFile() && sourceFile.isReadable() && sourceF.open(QFile::ReadOnly)){
        sourceData = sourceF.readAll();
        sourceF.close();
    }
    else {
        if (autoOption) sourceData.clear();
        else {
            qDebug().noquote() << sourceFile.absoluteFilePath() << " not readable, exiting...";
            return 1;
        }
    }
    QFile targetF(targetFile.absoluteFilePath());
    QByteArray targetData;
    bool targetNotExists = false;
    if (targetFile.isFile() && targetFile.isReadable() && targetF.open(QFile::ReadOnly)) {
        targetData = targetF.readAll();
        targetF.close();
    }
    else {
        if (autoOption) targetNotExists = true;
        else {
            qDebug().noquote() << targetFile.absoluteFilePath() << " not readable, exiting...";
            return 1;
        }
    }
    QFile patchF(patchFile.absoluteFilePath());
    QByteArray patchData;
    bool blank = false;
    if (patchFile.isFile() && patchFile.isReadable() && patchF.open(QFile::ReadOnly)) {
        patchData = patchF.readAll();
        patchF.close();
    }
    else {
        if (autoOption) {
            patchData = sourceData;
            blank = true;
        }
        else {
            qDebug().noquote() << patchFile.absoluteFilePath() << " not readable, exiting...";
            return 1;
        }
    }
    if (targetNotExists) {
        if (outFile.exists()) {
            QDir dir(outFile.path());
            if (dir.remove(outFile.absoluteFilePath())) qDebug().noquote() << "file removed" << outFile.absoluteFilePath();
            else {
                qDebug().noquote() << "failed to remove" << outFile.absoluteFilePath();
                return 1;
            }
        }
        else qDebug().noquote() << "nothing to do";
    }
    else if (ignoreOption && sourceData == targetData) {
        qDebug().noquote() << "no change - ignored";
    }
    else {
        QByteArray outData;
        //check for binary files
        if (sourceData.isEmpty() || sourceData.left(8000).contains((char) 0) || targetData.left(8000).contains((char) 0) || patchData.left(8000).contains((char) 0)) {
            qDebug().noquote() << "binary or empty source detected, full target data copied";
            outData = targetData;
        }
        else {
            QString sourceContent = QString::fromUtf8(sourceData),
                    targetContent = QString::fromUtf8(targetData),
                    patchContent = QString::fromUtf8(patchData);
            if (blank) patchContent.replace(QRegularExpression("[^\r\n]"), fill.left(1));
            //verify file size of patch file
            if (patchContent.length() != sourceContent.length()){
                if (forceOption) {
                    if (patchContent.length() > sourceContent.length()) {
                        int truncate = patchContent.length() - sourceContent.length();
                        patchContent.truncate(sourceContent.length());
                        qDebug().noquote() << "patch file truncated: " << truncate;
                    }
                    else {
                        int fCount = sourceContent.length()-patchContent.length();
                        patchContent.append(QString().fill(fill.at(0), fCount));
                        qDebug().noquote() << "patch file filled: " << fCount;
                    }
                }
                else {
                    qDebug().noquote() << "patch file size differs, force usage with -f, exiting...";
                    return 1;
                }
            }
            diff_match_patch dmp;
            QList<Patch> patches = dmp.patch_make(sourceContent, targetContent);
            qDebug().noquote().nospace() << "diff: " << sourceContent.left(50) << "...(" << sourceContent.length() << ") --> " << targetContent.left(50) << "...(" << targetContent.length() << ")";
            QStringList operations = {"DELETE", "INSERT", "EQUAL"};
            foreach( Patch patch, patches ) {
                int targetIndex = patch.start1;
                qDebug().noquote().nospace() << "@" << patch.start1 << " patch " << patch.length1 << "chars into " << patch.length2 << "chars";
                foreach( Diff diff, patch.diffs ) {
                    int count = diff.text.length();
                    switch (diff.operation) {
                        case DELETE:
                            patchContent.remove(targetIndex, count);
                            qDebug().noquote().nospace() << "deleted " << count << " chars @" << targetIndex;
                            break;
                        case INSERT:
                            patchContent.insert(targetIndex, diff.text);
                            qDebug().noquote().nospace() << "inserted \"" << diff.text.left(50) << "...\"(" << count << ") @" << targetIndex;
                            targetIndex += count;
                            break;
                        case EQUAL:
                            targetIndex += count;
                            qDebug().noquote().nospace() << "skipped " << count << " chars";
                            break;
                    }
                }
            }
            if (patchContent.length() != targetContent.length()) {
                qDebug().noquote() << "something went wrong! length mismatch: target = " << targetContent.length() << " , out = " << patchContent.length();
                return 1;
            }
            outData = patchContent.toUtf8();
        }
        if (!QFileInfo(outFile.absoluteFilePath()).absoluteDir().exists()) {
            if (QDir().mkpath(QFileInfo(outFile.absoluteFilePath()).absoluteDir().absolutePath())) qDebug().noquote() << "dir created: " << QFileInfo(outFile.absoluteFilePath()).absoluteDir().absolutePath();
            else {
                qDebug().noquote() << "failed to create dir: " << QFileInfo(outFile.absoluteFilePath()).absoluteDir().absolutePath();
                return 1;
            }
        }
        QFile outF(outFile.absoluteFilePath());
        if (outF.open(QFile::WriteOnly)){
            outF.write(outData);
            outF.close();
        }
        else {
            qDebug().noquote() << "failed to write to file: " << outFile.absoluteFilePath();
            return 1;
        }
    }
    return 0;
}

int main(int argc, char *argv[])
{
    //evaluate commandline arguments
    QCoreApplication a(argc, argv);
    QCommandLineParser parser;
    parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);
    const QCommandLineOption sourceOption("s", "The diff source file", "source", "source.txt");
    parser.addOption(sourceOption);
    const QCommandLineOption targetOption("t", "The diff target file", "target", "target.txt");
    parser.addOption(targetOption);
    const QCommandLineOption patchOption("p", "The file to be patched", "patch", "patch.txt");
    parser.addOption(patchOption);
    const QCommandLineOption outOption("o", "The output file", "out", "out.txt");
    parser.addOption(outOption);
    const QCommandLineOption autoOption(QStringList() << "a" << "auto", "auto create/delete files");
    parser.addOption(autoOption);
    const QCommandLineOption dirsOption(QStringList() << "d" << "dirs", "handle as directories (ignore hidden folders)");
    parser.addOption(dirsOption);
    const QCommandLineOption ignoreOption(QStringList() << "i" << "ignore", "ignore unchanged files");
    parser.addOption(ignoreOption);
    const QCommandLineOption fillOption("c", "character used for filling", "fillchar", " ");;
    parser.addOption(fillOption);
    const QCommandLineOption forceOption(QStringList() << "f" << "force", "Force patch file");
    parser.addOption(forceOption);
    const QCommandLineOption helpOption = parser.addHelpOption();
    const QCommandLineOption versionOption = parser.addVersionOption();

    if (parser.parse(QCoreApplication::arguments())) {
        //check directories and files
        QString sourceFilePath(parser.value(sourceOption)),
                targetFilePath(parser.value(targetOption)),
                patchFilePath(parser.value(patchOption)),
                outFilePath(parser.value(outOption));
        if (parser.isSet(dirsOption)) {
            sourceFilePath.append("/");
            targetFilePath.append("/");
            patchFilePath.append("/");
            outFilePath.append("/");
        }
        QFileInfo sourceFile(sourceFilePath),
                  targetFile(targetFilePath),
                  patchFile(patchFilePath),
                  outFile(outFilePath);
        bool argumentsOk = (!parser.isSet(dirsOption) && ((sourceFile.isFile() && sourceFile.isReadable()) || parser.isSet(autoOption))
                         && ((targetFile.isFile() && targetFile.isReadable()) || parser.isSet(autoOption))
                         && ((patchFile.isFile() && patchFile.isReadable()) || parser.isSet(autoOption))
                         && (!outFile.exists() || (outFile.isFile() && outFile.isWritable())))
                        || (parser.isSet(dirsOption) && sourceFile.isDir() && targetFile.isDir() && patchFile.isDir() && outFile.isDir());
        if (!argumentsOk) {
            qDebug().noquote() << sourceFile.absoluteFilePath() << ((!parser.isSet(dirsOption) && ((sourceFile.isFile() && sourceFile.isReadable()) || parser.isSet(autoOption))) || (parser.isSet(dirsOption) && sourceFile.isDir()));
            qDebug().noquote() << targetFile.absoluteFilePath() << ((!parser.isSet(dirsOption) && ((targetFile.isFile() && targetFile.isReadable()) || parser.isSet(autoOption))) || (parser.isSet(dirsOption) && targetFile.isDir()));;
            qDebug().noquote() << patchFile.absoluteFilePath() << ((!parser.isSet(dirsOption) && ((patchFile.isFile() && patchFile.isReadable()) || parser.isSet(autoOption))) || (parser.isSet(dirsOption) && patchFile.isDir()));;
            qDebug().noquote() << outFile.absoluteFilePath() << ((!parser.isSet(dirsOption) && (!outFile.exists() || (outFile.isFile() && outFile.isWritable()))) || (parser.isSet(dirsOption) && outFile.isDir()));;
            printf("argument error\n\n");
        }
        //print version
        if (!argumentsOk || parser.isSet(helpOption) || parser.isSet(versionOption))
            printf("DiffMorpher version 1.0\n\n");
        //print help
        if (!argumentsOk || parser.isSet(helpOption))
            printf("usage: diffmorpher\n"
                    "   options:\n"
                    "      -s, --source     Source file for diffs\n"
                    "      -t, --target     Target file for diffs\n"
                    "      -p, --patch      File to apply patches to\n"
                    "      -o, --out        Output file\n"
                    "      -a, --auto       auto delete/create files which are non existing on either side\n"
                    "      -c, --fillchar   character used for filling\n"
                    "      -d, --dirs       handle as directories (ignore hidden folders)\n"
                    "      -i, --ignore     ignore unchanged files\n"
                    "      -f, --force      force patching different file length (truncate or pad with space)\n\n\n");
        //run program
        if (argumentsOk) {
            if (parser.isSet(dirsOption)) {
                QStringList files;
                {
                    QDirIterator dit(sourceFile.absoluteFilePath(), QDir::Files, QDirIterator::Subdirectories);
                    while (dit.hasNext()) files.append(dit.next().remove(0,sourceFile.absoluteFilePath().length()));
                }
                {
                    QDirIterator dit(targetFile.absoluteFilePath(), QDir::Files, QDirIterator::Subdirectories);
                    while (dit.hasNext()) files.append(dit.next().remove(0,targetFile.absoluteFilePath().length()));
                }
                files.removeDuplicates();
                qDebug().noquote() << "handle files: " << files;
                foreach (QString file, files) {
                    qDebug().noquote() << "------------------";
                    qDebug().noquote() << file;
                    if (handleFiles(QFileInfo(sourceFile.absoluteFilePath().append(file)), QFileInfo(targetFile.absoluteFilePath().append(file)), QFileInfo(patchFile.absoluteFilePath().append(file)), QFileInfo(outFile.absoluteFilePath().append(file)), parser.isSet(autoOption), parser.isSet(forceOption), parser.isSet(ignoreOption), parser.value(fillOption))) return 1;
                }
            }
            else return handleFiles(sourceFile, targetFile, patchFile, outFile, parser.isSet(autoOption), parser.isSet(forceOption), parser.isSet(ignoreOption), parser.value(fillOption));
        }
    }
    else {
        qDebug().noquote() << parser.errorText();
        return 1;
    }
}

