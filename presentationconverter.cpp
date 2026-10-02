/*
 * Copyright (C) 2026 Michael Speckenbach
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#include "presentationconverter.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUrl>
#include <QXmlStreamWriter>

namespace {

constexpr int kTimeoutMs = 120000;   // per converter

QString cacheDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/presentations";
}

QString cachePathFor(const QString &source)
{
    const QFileInfo info(source);
    // Cache key: path + modification time -> reconvert when the file changed
    const QByteArray key = QCryptographicHash::hash(
        (info.absoluteFilePath() + info.lastModified().toString(Qt::ISODate)).toUtf8(),
        QCryptographicHash::Sha1).toHex();
    return cacheDir() + "/" + QString::fromLatin1(key) + ".pdf";
}

QString firstExisting(const QStringList &candidates)
{
    for (const QString &path : candidates) {
        if (!path.isEmpty() && QFileInfo::exists(path)) {
            return path;
        }
    }
    return QString();
}

// ---------------------------------------------------------------------------
// Job + process helper
// ---------------------------------------------------------------------------

struct Job {
    QString source;                 // absolute path of the presentation
    QString target;                 // PDF to create
    QString workDir;                // empty temporary directory for this job
    const std::atomic_bool *cancel;
};

enum class Outcome { NotAvailable, Failed, Ok };

// Runs a program synchronously (we are in the worker thread), with timeout and cancel support
bool runProcess(const Job &job, const QString &program, const QStringList &args, QString *log)
{
    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.setWorkingDirectory(job.workDir);
    proc.start(program, args);
    if (!proc.waitForStarted(10000)) {
        *log = proc.errorString();
        return false;
    }

    QElapsedTimer timer;
    timer.start();
    while (!proc.waitForFinished(200)) {
        if (proc.state() == QProcess::NotRunning) {
            break;
        }
        if (job.cancel->load() || timer.elapsed() > kTimeoutMs) {
            proc.kill();
            proc.waitForFinished(2000);
            *log = job.cancel->load() ? QCoreApplication::translate("PresentationConverter", "cancelled") : QCoreApplication::translate("PresentationConverter", "timeout");
            return false;
        }
    }

    *log = QString::fromLocal8Bit(proc.readAll()).trimmed();
    return proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0;
}

bool writeTextFile(const QString &path, const QString &text)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    file.write(text.toUtf8());
    return true;
}

// Moves 'produced' to the job target. Returns Ok only if a non-empty PDF exists.
Outcome takeResult(const Job &job, const QString &produced, const QString &log, bool ran, QString *message)
{
    if (ran && QFileInfo(produced).size() > 0) {
        QFile::remove(job.target);
        if (QFile::rename(produced, job.target) || QFile::copy(produced, job.target)) {
            return Outcome::Ok;
        }
    }
    *message = log.isEmpty() ? QCoreApplication::translate("PresentationConverter", "no PDF was created") : log;
    return Outcome::Failed;
}

// ---------------------------------------------------------------------------
// Microsoft PowerPoint (Windows): COM automation via PowerShell
// ---------------------------------------------------------------------------

#ifdef Q_OS_WIN
Outcome convertWithPowerPointWindows(const Job &job, QString *message)
{
    // PowerPoint registers its COM class here; no PowerPoint -> no entry
    const QSettings reg("HKEY_CLASSES_ROOT\\PowerPoint.Application\\CLSID", QSettings::NativeFormat);
    if (reg.value("Default").toString().isEmpty()) {
        return Outcome::NotAvailable;
    }

    // If PowerPoint is already open (someone is working with it) we must not quit it
    const QString script = QStringLiteral(R"PS(
$ErrorActionPreference = 'Stop'
$in  = $args[0]
$out = $args[1]
$wasRunning = [bool](Get-Process POWERPNT -ErrorAction SilentlyContinue)
$ppt = New-Object -ComObject PowerPoint.Application
try {
    # Open(FileName, ReadOnly = msoTrue, Untitled = msoFalse, WithWindow = msoFalse)
    $pres = $ppt.Presentations.Open($in, -1, 0, 0)
    try {
        $pres.SaveAs($out, 32)   # 32 = ppSaveAsPDF
    } finally {
        $pres.Close()
    }
} finally {
    if (-not $wasRunning) { $ppt.Quit() }
    [void][System.Runtime.InteropServices.Marshal]::ReleaseComObject($ppt)
}
)PS");

    const QString scriptPath = job.workDir + "/convert.ps1";
    if (!writeTextFile(scriptPath, script)) {
        *message = QCoreApplication::translate("PresentationConverter", "cannot write helper script");
        return Outcome::Failed;
    }

    const QString produced = job.workDir + "/powerpoint.pdf";
    QString log;
    const bool ran = runProcess(job, "powershell.exe",
                                {"-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass",
                                 "-File", QDir::toNativeSeparators(scriptPath),
                                 QDir::toNativeSeparators(job.source),
                                 QDir::toNativeSeparators(produced)},
                                &log);
    return takeResult(job, produced, log, ran, message);
}
#endif

// ---------------------------------------------------------------------------
// Microsoft PowerPoint + Keynote (macOS): AppleScript via osascript
// ---------------------------------------------------------------------------

#ifdef Q_OS_MACOS
// PowerPoint and Keynote are sandboxed: they ask the user for access to every folder they
// did not open themselves. A fixed folder (instead of a new temp dir every time) means they
// ask only once and remember the permission.
QString sandboxedWorkDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/conversion";
}

// Only the files: the folder itself stays, the permission belongs to it
void emptyFolder(const QString &dir)
{
    for (const QString &file : QDir(dir).entryList(QDir::Files | QDir::Hidden)) {
        QFile::remove(dir + "/" + file);
    }
}

Outcome runAppleScript(const Job &job, const QString &script, const QString &pdfName, QString *message)
{
    // NOTE: On first use macOS asks the user to allow ChurchWorshipM to control the app,
    // and the app asks once for access to the work folder.
    const QString scriptPath = job.workDir + "/convert.applescript";   // read by osascript only
    if (!writeTextFile(scriptPath, script)) {
        *message = QCoreApplication::translate("PresentationConverter", "cannot write helper script");
        return Outcome::Failed;
    }
    // The presentation goes into the work folder as well: files elsewhere (USB stick,
    // our library) would cause another question
    const QString workDir = sandboxedWorkDir();
    emptyFolder(workDir);   // leftovers of a cancelled conversion
    if (!QDir().mkpath(workDir)) {
        *message = QCoreApplication::translate("PresentationConverter", "cannot create the work folder");
        return Outcome::Failed;
    }
    const QString input = workDir + "/presentation." + QFileInfo(job.source).suffix().toLower();
    if (!QFile::copy(job.source, input)) {
        *message = QCoreApplication::translate("PresentationConverter", "cannot copy the presentation");
        return Outcome::Failed;
    }
    const QString produced = workDir + "/" + pdfName;

    QString log;
    const bool ran = runProcess(job, "/usr/bin/osascript", {scriptPath, input, produced}, &log);
    const Outcome outcome = takeResult(job, produced, log, ran, message);
    emptyFolder(workDir);
    return outcome;
}

Outcome convertWithPowerPointMac(const Job &job, QString *message)
{
    if (!QFileInfo::exists("/Applications/Microsoft PowerPoint.app")) {
        return Outcome::NotAvailable;
    }
    const QString script = QStringLiteral(R"AS(
on run argv
    set inFile to POSIX file (item 1 of argv)
    set outFile to POSIX file (item 2 of argv)
    set wasRunning to application "Microsoft PowerPoint" is running
    tell application "Microsoft PowerPoint"
        open inFile
        set pres to active presentation
        save pres in outFile as save as PDF
        close pres saving no
        if not wasRunning then quit
    end tell
end run
)AS");
    return runAppleScript(job, script, "powerpoint.pdf", message);
}

Outcome convertWithKeynote(const Job &job, QString *message)
{
    if (!QFileInfo::exists("/Applications/Keynote.app")) {
        return Outcome::NotAvailable;
    }
    const QString script = QStringLiteral(R"AS(
on run argv
    set inFile to POSIX file (item 1 of argv)
    set outFile to POSIX file (item 2 of argv)
    set wasRunning to application "Keynote" is running
    tell application "Keynote"
        set doc to open inFile
        export doc to outFile as PDF
        close doc saving no
        if not wasRunning then quit
    end tell
end run
)AS");
    return runAppleScript(job, script, "keynote.pdf", message);
}
#endif

// ---------------------------------------------------------------------------
// OnlyOffice: the converter 'x2t' shipped with the desktop editors
// ---------------------------------------------------------------------------

QString findOnlyOfficeX2t()
{
    QStringList candidates;
#if defined(Q_OS_WIN)
    for (const char *env : {"ProgramFiles", "ProgramFiles(x86)"}) {
        const QString base = qEnvironmentVariable(env);
        if (!base.isEmpty()) {
            candidates << base + "/ONLYOFFICE/DesktopEditors/converter/x2t.exe";
        }
    }
#elif defined(Q_OS_MACOS)
    candidates << "/Applications/ONLYOFFICE.app/Contents/Resources/converter/x2t"
               << "/Applications/ONLYOFFICE.app/Contents/Frameworks/converter/x2t"
               << "/Applications/ONLYOFFICE.app/Contents/MacOS/converter/x2t";
#else
    candidates << "/opt/onlyoffice/desktopeditors/converter/x2t"
               << "/usr/lib/onlyoffice/desktopeditors/converter/x2t"
               << "/usr/share/onlyoffice/desktopeditors/converter/x2t";
#endif
    return firstExisting(candidates);
}

// Font table created by the desktop editors on their first start
QString findOnlyOfficeAllFonts()
{
    QStringList candidates;
    for (const QString &base : QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation)) {
        candidates << base + "/onlyoffice/desktopeditors/data/fonts/AllFonts.js"   // Linux
                   << base + "/ONLYOFFICE/DesktopEditors/data/fonts/AllFonts.js";  // Windows
    }
#ifdef Q_OS_MACOS
    candidates << QDir::homePath() + "/Library/Containers/asc.onlyoffice.ONLYOFFICE/Data/Library/"
                                     "Application Support/asc.onlyoffice.ONLYOFFICE/data/fonts/AllFonts.js";
#endif
    return firstExisting(candidates);
}

Outcome convertWithOnlyOffice(const Job &job, QString *message)
{
    const QString x2t = findOnlyOfficeX2t();
    if (x2t.isEmpty()) {
        return Outcome::NotAvailable;
    }
    const QString allFonts = findOnlyOfficeAllFonts();
    if (allFonts.isEmpty()) {
        *message = QCoreApplication::translate("PresentationConverter", "font table not found - please start OnlyOffice once");
        return Outcome::Failed;
    }

    // "fonts" folder of the installation (converter/../fonts)
    const QString fontDir = QFileInfo(x2t).dir().absoluteFilePath("../fonts");
    const QString produced = job.workDir + "/onlyoffice.pdf";
    const QString tempDir  = job.workDir + "/x2t";
    QDir().mkpath(tempDir);

    // x2t is controlled by a parameter file (same format OnlyOffice uses internally)
    const QString paramsPath = job.workDir + "/params.xml";
    QFile params(paramsPath);
    if (!params.open(QIODevice::WriteOnly)) {
        *message = QCoreApplication::translate("PresentationConverter", "cannot write helper script");
        return Outcome::Failed;
    }
    QXmlStreamWriter xml(&params);
    xml.writeStartDocument();
    xml.writeStartElement("TaskQueueDataConvert");
    xml.writeTextElement("m_sFileFrom", QDir::toNativeSeparators(job.source));
    xml.writeTextElement("m_sFileTo", QDir::toNativeSeparators(produced));
    xml.writeTextElement("m_nFormatTo", "513");   // 513 = PDF
    xml.writeTextElement("m_sFontDir", QDir::toNativeSeparators(fontDir));
    xml.writeTextElement("m_sAllFontsPath", QDir::toNativeSeparators(allFonts));
    xml.writeTextElement("m_sTempDir", QDir::toNativeSeparators(tempDir));
    xml.writeTextElement("m_bIsNoBase64", "true");
    xml.writeEndElement();
    xml.writeEndDocument();
    params.close();

    QString log;
    const bool ran = runProcess(job, x2t, {QDir::toNativeSeparators(paramsPath)}, &log);
    return takeResult(job, produced, log, ran, message);
}

// ---------------------------------------------------------------------------
// LibreOffice: soffice --headless --convert-to pdf
// ---------------------------------------------------------------------------

QString findLibreOffice()
{
    QStringList candidates;
#if defined(Q_OS_WIN)
    for (const char *env : {"ProgramFiles", "ProgramFiles(x86)"}) {
        const QString base = qEnvironmentVariable(env);
        if (!base.isEmpty()) {
            // soffice.com waits for the conversion, soffice.exe may return early
            candidates << base + "/LibreOffice/program/soffice.com"
                       << base + "/LibreOffice/program/soffice.exe";
        }
    }
#elif defined(Q_OS_MACOS)
    candidates << "/Applications/LibreOffice.app/Contents/MacOS/soffice";
#endif
    candidates << QStandardPaths::findExecutable("soffice")
               << QStandardPaths::findExecutable("libreoffice");
    return firstExisting(candidates);
}

Outcome convertWithLibreOffice(const Job &job, QString *message)
{
    const QString office = findLibreOffice();
    if (office.isEmpty()) {
        return Outcome::NotAvailable;
    }

    // Own profile, so the conversion also works while LibreOffice is open on the desktop
    const QString profile = QUrl::fromLocalFile(
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/lo_profile").toString();

    const QString outDir = job.workDir + "/libreoffice";
    QDir().mkpath(outDir);

    QString log;
    const bool ran = runProcess(job, office,
                                {"-env:UserInstallation=" + profile,
                                 "--headless", "--convert-to", "pdf",
                                 "--outdir", QDir::toNativeSeparators(outDir),
                                 QDir::toNativeSeparators(job.source)},
                                &log);
    const QString produced = outDir + "/" + QFileInfo(job.source).completeBaseName() + ".pdf";
    return takeResult(job, produced, log, ran, message);
}

// ---------------------------------------------------------------------------
// Chain
// ---------------------------------------------------------------------------

struct Converter {
    const char *name;
    Outcome (*run)(const Job &, QString *);
};

QList<Converter> convertersForPlatform()
{
    return {
#if defined(Q_OS_WIN)
        {"PowerPoint",  convertWithPowerPointWindows},
#elif defined(Q_OS_MACOS)
        {"PowerPoint",  convertWithPowerPointMac},
        {"Keynote",     convertWithKeynote},
#endif
        {"OnlyOffice",  convertWithOnlyOffice},
        {"LibreOffice", convertWithLibreOffice},
    };
}

// Runs in the worker thread. Returns an error text, empty on success.
QString runChain(const QString &source, const QString &target, const std::atomic_bool *cancel)
{
    QStringList failures;
    for (const Converter &conv : convertersForPlatform()) {
        if (cancel->load()) {
            return QCoreApplication::translate("PresentationConverter", "cancelled");
        }

        QTemporaryDir workDir;
        if (!workDir.isValid()) {
            return QCoreApplication::translate("PresentationConverter", "Cannot create a temporary folder.");
        }
        const Job job{QFileInfo(source).absoluteFilePath(), target, workDir.path(), cancel};

        QString message;
        switch (conv.run(job, &message)) {
        case Outcome::Ok:
            return QString();
        case Outcome::Failed:
            failures << QStringLiteral("%1: %2").arg(QLatin1String(conv.name), message);
            break;
        case Outcome::NotAvailable:
            break;
        }
    }

    if (failures.isEmpty()) {
        return QCoreApplication::translate("PresentationConverter", "No program for converting presentations was found.\n"
                  "Please install PowerPoint, OnlyOffice or LibreOffice, "
                  "or export the presentation as PDF.");
    }
    return QCoreApplication::translate("PresentationConverter", "The presentation could not be converted.") + "\n\n" + failures.join('\n');
}

} // namespace

// ---------------------------------------------------------------------------
// PresentationConverter
// ---------------------------------------------------------------------------

PresentationConverter::PresentationConverter(QObject *parent)
    : QObject(parent)
{
    m_pool.setMaxThreadCount(1);   // one conversion after another
    QDir().mkpath(cacheDir());
}

PresentationConverter::~PresentationConverter()
{
    // Don't wait minutes for a slow office program when the app is closed
    m_cancel = true;
    m_pool.clear();
    m_pool.waitForDone();
}

QString PresentationConverter::cachedPdf(const QString &source)
{
    const QString path = cachePathFor(source);
    return QFileInfo(path).size() > 0 ? path : QString();
}

void PresentationConverter::convert(const QString &source)
{
    if (source.isEmpty() || m_pending.contains(source) || !cachedPdf(source).isEmpty()) {
        return;
    }
    m_pending.insert(source);

    const QString target = cachePathFor(source);
    m_pool.start([this, source, target] {
        // Convert into a temp name first: a half written file must never look like a cache hit
        const QString partial = target + ".part";
        QString error = runChain(source, partial, &m_cancel);
        if (error.isEmpty()) {
            QFile::remove(target);
            if (!QFile::rename(partial, target)) {
                error = QCoreApplication::translate("PresentationConverter", "The converted file could not be stored in the cache.");
            }
        }
        QFile::remove(partial);

        // Back to the GUI thread
        QMetaObject::invokeMethod(this, [this, source, error] {
            m_pending.remove(source);
            emit finished(source, error);
        }, Qt::QueuedConnection);
    });
}
