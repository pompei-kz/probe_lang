CREATE USER probe_lang_user WITH PASSWORD 'YjV2bV1CGpTWT0fMDV2H';
CREATE DATABASE probe_lang_db OWNER probe_lang_user;
GRANT ALL PRIVILEGES ON DATABASE probe_lang_db TO probe_lang_user;
