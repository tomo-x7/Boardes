#include "zipio.h"

#include <cstring>

ZipReader::ZipReader(const QByteArray &data) : m_ownedData(data) {
	std::memset(&m_zip, 0, sizeof(m_zip));
	m_valid = mz_zip_reader_init_mem(&m_zip, m_ownedData.constData(),
									 static_cast<size_t>(m_ownedData.size()), 0);
	if (!m_valid) {
		m_error = QString::fromUtf8(mz_zip_get_error_string(mz_zip_get_last_error(&m_zip)));
	}
}

ZipReader::ZipReader(const QString &filePath) {
	std::memset(&m_zip, 0, sizeof(m_zip));
	const QByteArray pathUtf8 = filePath.toUtf8();
	m_valid = mz_zip_reader_init_file(&m_zip, pathUtf8.constData(), 0);
	if (!m_valid) {
		m_error = QString::fromUtf8(mz_zip_get_error_string(mz_zip_get_last_error(&m_zip)));
	}
}

ZipReader::~ZipReader() {
	if (m_valid) {
		mz_zip_reader_end(&m_zip);
	}
}

QStringList ZipReader::fileNames() const {
	QStringList out;
	if (!m_valid) {
		return out;
	}
	const mz_uint n = mz_zip_reader_get_num_files(&m_zip);
	for (mz_uint i = 0; i < n; ++i) {
		if (mz_zip_reader_is_file_a_directory(&m_zip, i)) {
			continue;
		}
		mz_zip_archive_file_stat stat;
		if (!mz_zip_reader_file_stat(&m_zip, i, &stat)) {
			continue;
		}
		out.append(QString::fromUtf8(stat.m_filename));
	}
	return out;
}

bool ZipReader::contains(const QString &path) const {
	if (!m_valid) {
		return false;
	}
	const QByteArray pathUtf8 = path.toUtf8();
	return mz_zip_reader_locate_file(&m_zip, pathUtf8.constData(), nullptr, 0) >= 0;
}

QByteArray ZipReader::read(const QString &path, bool *ok) const {
	if (ok) {
		*ok = false;
	}
	if (!m_valid) {
		return {};
	}
	const QByteArray pathUtf8 = path.toUtf8();
	const int index = mz_zip_reader_locate_file(&m_zip, pathUtf8.constData(), nullptr, 0);
	if (index < 0) {
		return {};
	}
	size_t size = 0;
	void *buf = mz_zip_reader_extract_to_heap(&m_zip, static_cast<mz_uint>(index), &size, 0);
	if (!buf) {
		return {};
	}
	QByteArray result(reinterpret_cast<const char *>(buf), static_cast<int>(size));
	mz_free(buf);
	if (ok) {
		*ok = true;
	}
	return result;
}

ZipWriter::ZipWriter() {
	std::memset(&m_zip, 0, sizeof(m_zip));
	m_initOk = mz_zip_writer_init_heap(&m_zip, 0, 128 * 1024);
	m_ok = m_initOk;
	if (!m_ok) {
		m_error = QStringLiteral("zip writer の初期化に失敗しました");
	}
}

ZipWriter::~ZipWriter() {
	// init が成功していれば、finalize の成否によらず必ず end() で内部状態を解放する。
	if (m_initOk) {
		mz_zip_writer_end(&m_zip);
	}
}

bool ZipWriter::addFile(const QString &path, const QByteArray &data, int compressionLevel) {
	if (!m_ok || m_finished) {
		return false;
	}
	const QByteArray pathUtf8 = path.toUtf8();
	const mz_bool result = mz_zip_writer_add_mem(&m_zip, pathUtf8.constData(), data.constData(),
												 static_cast<size_t>(data.size()),
												 static_cast<mz_uint>(compressionLevel));
	if (!result) {
		m_error = QStringLiteral("zip へのファイル追加に失敗しました: %1").arg(path);
		m_ok = false;
		return false;
	}
	return true;
}

QByteArray ZipWriter::finish() {
	if (!m_ok || m_finished) {
		return {};
	}
	void *buf = nullptr;
	size_t size = 0;
	const mz_bool result = mz_zip_writer_finalize_heap_archive(&m_zip, &buf, &size);
	m_finished = true;
	if (!result || !buf) {
		m_error = QStringLiteral("zip の確定に失敗しました");
		m_ok = false;
		return {};
	}
	QByteArray out(reinterpret_cast<const char *>(buf), static_cast<int>(size));
	mz_free(buf);
	return out;
}
