document.addEventListener('DOMContentLoaded', () => {
    const registerForm = document.getElementById('registerForm');
    const loginForm = document.getElementById('loginForm');

    function hashPassword(password) {
        return CryptoJS.SHA256(password).toString();
    }

    registerForm.addEventListener('submit', async (e) => {
        e.preventDefault();
        const username = document.getElementById('regUsername').value;
        const password = document.getElementById('regPassword').value;
        
        try {
            const response = await fetch('http://localhost:3000/users', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({
                    username,
                    password: hashPassword(password)
                }),
            });
            
            if (response.ok) {
                alert('Registration successful!');
                registerForm.reset();
            } else {
                alert('Registration failed!');
            }
        } catch (error) {
            console.error('Error:', error);
            alert('Registration failed!');
        }
    });

    loginForm.addEventListener('submit', async (e) => {
        e.preventDefault();
        const username = document.getElementById('loginUsername').value;
        const password = document.getElementById('loginPassword').value;
        
        try {
            const response = await fetch(`http://localhost:3000/users?username=${username}`);
            const users = await response.json();
            const user = users[0];
            
            if (user && user.password === hashPassword(password)) {
                alert('Login successful!');
                loginForm.reset();
            } else {
                alert('Invalid credentials!');
            }
        } catch (error) {
            console.error('Error:', error);
            alert('Login failed!');
        }
    });
});