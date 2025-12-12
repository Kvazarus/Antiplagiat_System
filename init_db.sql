CREATE TABLE IF NOT EXISTS submissions (
    id SERIAL PRIMARY KEY,
    student_name VARCHAR(255) NOT NULL,
    task_id VARCHAR(50) NOT NULL,
    filename VARCHAR(255) NOT NULL,
    upload_timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS reports (
    id SERIAL PRIMARY KEY,
    submission_id INT REFERENCES submissions(id),
    is_plagiarism BOOLEAN DEFAULT FALSE,
    similar_to_id INT,
    check_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);