CREATE USER probe_lang_test_user_01 WITH PASSWORD 'MHvYBQ2u2uJgmPCfePvQ';
CREATE DATABASE probe_lang_test_db_01 OWNER probe_lang_test_user_01;
GRANT ALL PRIVILEGES ON DATABASE probe_lang_test_db_01 TO probe_lang_test_user_01;
