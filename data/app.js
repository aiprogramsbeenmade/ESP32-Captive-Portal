document.addEventListener("DOMContentLoaded", () => {
    const form           = document.getElementById("setup-form");
    const submitBtn       = document.getElementById("submit-btn");
    const errorMsg        = document.getElementById("error-msg");
    const formScreen      = document.getElementById("form-screen");
    const successScreen   = document.getElementById("success-screen");

    form.addEventListener("submit", async (event) => {
        event.preventDefault();
        hideError();
        setLoading(true);

        const payload = {
            email: document.getElementById("email").value.trim(),
            password: document.getElementById("password").value
        };


        if (!payload.email || !payload.password) {
            showError("Email e Password sono obbligatori.");
            setLoading(false);
            return;
        }

        try {
            const response = await fetch("/api/submit", {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify(payload)
            });

            if (!response.ok) {
                const data = await safeJson(response);
                throw new Error(data?.error || `Errore server (${response.status})`);
            }

            // Configurazione completata: mostra la schermata di successo
            formScreen.classList.add("hidden");
            successScreen.classList.remove("hidden");
        } catch (err) {
            showError(err.message || "Errore di rete, riprova.");
        } finally {
            setLoading(false);
        }
    });

    function setLoading(isLoading) {
        submitBtn.disabled = isLoading;
        submitBtn.textContent = isLoading ? "Attendi..." : "Avanti";
    }

    function showError(message) {
        errorMsg.textContent = message;
        errorMsg.classList.remove("hidden");
    }

    function hideError() {
        errorMsg.classList.add("hidden");
    }

    async function safeJson(response) {
        try {
            return await response.json();
        } catch {
            return null;
        }
    }
});
