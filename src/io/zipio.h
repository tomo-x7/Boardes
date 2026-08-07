#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

#include "../core/util.h"
#include "../vendor/miniz.h"

// miniz を薄くラップした zip 読み書き。miniz を直接触るのはこのファイルだけにする。
class ZipReader {
public:
	explicit ZipReader(const QByteArray &data);
	explicit ZipReader(const QString &filePath);
	~ZipReader();

	NONCOPYABLE(ZipReader)
	NONMOVABLE(ZipReader)

	bool isValid() const {
		return m_valid;
	}
	QString errorString() const {
		return m_error;
	}

	// ディレクトリエントリを除いたファイル一覧。
	QStringList fileNames() const;
	bool contains(const QString &path) const;
	// 読み込みに失敗した場合、ok が渡されていれば false を書き込み空配列を返す。
	QByteArray read(const QString &path, bool *ok = nullptr) const;

private:
	// miniz の mz_zip_reader_* 系関数は読み取り専用の操作でも非 const ポインタを要求するため、
	// ZipReader の const メンバ関数 (fileNames/contains/read) から呼べるよう mutable にする。
	mutable mz_zip_archive m_zip;
	bool m_valid = false;
	QByteArray m_ownedData;  // メモリコンストラクタの場合、archive が参照し続けるため保持
	QString m_error;
};

class ZipWriter {
public:
	ZipWriter();
	~ZipWriter();

	NONCOPYABLE(ZipWriter)
	NONMOVABLE(ZipWriter)

	bool isValid() const {
		return m_ok;
	}
	QString errorString() const {
		return m_error;
	}

	// path 区切りは '/' を使うこと (zip の慣習)。
	bool addFile(const QString &path, const QByteArray &data, int compressionLevel = 6);

	// アーカイブを確定してバイト列を返す。一度呼ぶと以後 addFile は呼べない。
	// 失敗時は isValid() が false になる。
	QByteArray finish();

private:
	mz_zip_archive m_zip;
	bool m_initOk = false;  // mz_zip_writer_init_heap が成功したか (デストラクタで end() を呼ぶ判断に使う)
	bool m_finished = false;
	bool m_ok = true;
	QString m_error;
};
