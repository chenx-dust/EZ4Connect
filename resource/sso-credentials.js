// Runs only in the main frame. Values are passed as JSON by the native UI.
(function (request) {
    "use strict";
    if (window !== window.top || location.protocol !== "https:" || location.origin !== request.origin) {
        return {status: "origin-mismatch"};
    }
    const editable = element => {
        if (element.disabled || element.readOnly || !element.getClientRects().length) return false;
        for (let node = element; node; node = node.parentElement) {
            const style = getComputedStyle(node);
            if (style.visibility !== "visible" || style.display === "none" || Number(style.opacity) === 0) return false;
        }
        return true;
    };
    const inputs = Array.from(document.querySelectorAll("input")).filter(editable);
    const passwords = inputs.filter(element => element.type === "password");
    if (passwords.length !== 1 || passwords[0].autocomplete === "new-password") {
        return {status: "no-login-form"};
    }
    const password = passwords[0];
    const description = element => [element.name, element.id, element.placeholder,
        element.getAttribute("aria-label"), element.autocomplete].join(" ");
    if (/otp|one.?time|verification|captcha|new.?password|confirm|验证码|动态|新密码|确认密码/i.test(description(password))) {
        return {status: "no-login-form"};
    }
    const sameOriginAction = action => {
        try {
            const target = new URL(action || location.href, location.href);
            return target.protocol === "https:" && target.origin === request.origin;
        } catch (_) {
            return false;
        }
    };
    const safeForm = form => !form || (sameOriginAction(form.action) &&
        Array.from(document.querySelectorAll("button,input")).every(control =>
            control.form !== form || !["submit", "image"].includes(control.type) ||
            !control.hasAttribute("formaction") || sameOriginAction(control.formAction)));
    const form = password.form;
    if (!safeForm(form)) {
        return {status: "origin-mismatch"};
    }
    const candidates = inputs.filter(element =>
        ["text", "email", "tel"].includes(element.type) && element.form === form &&
        !!(element.compareDocumentPosition(password) & Node.DOCUMENT_POSITION_FOLLOWING) &&
        !/captcha|verification|code|otp|search|验证码|动态口令/i.test(description(element)) &&
        !element.autocomplete.includes("one-time-code"));
    const explicit = candidates.filter(element => element.autocomplete.split(/\s+/).includes("username"));
    const named = candidates.filter(element => /user|account|login|email|phone|mobile|用户名|帐号|账号|工号|手机|邮箱/i.test(description(element)));
    const choices = explicit.length ? explicit : named.length ? named : candidates;
    if (choices.length !== 1) {
        return {status: "no-login-form"};
    }
    const username = choices[0];
    if (request.operation === "inspect") {
        return {status: "ready"};
    }
    if (request.operation === "capture") {
        if (!username.value.trim() || !password.value) return {status: "empty"};
        return {status: "captured", username: username.value, password: password.value};
    }
    if (request.operation !== "fill") return {status: "invalid-operation"};
    if (username.value && username.value !== request.username) return {status: "different-account"};
    if (password.value) return {status: "already-filled"};
    const setter = Object.getOwnPropertyDescriptor(HTMLInputElement.prototype, "value").set;
    const setValue = (element, value) => {
        setter.call(element, value);
        element.dispatchEvent(new Event("input", {bubbles: true}));
        element.dispatchEvent(new Event("change", {bubbles: true}));
    };
    if (!username.value) setValue(username, request.username);
    // An input handler may navigate or alter the form. Recheck before inserting the password.
    if (location.origin !== request.origin || !username.isConnected || !editable(username) ||
        !["text", "email", "tel"].includes(username.type) ||
        username.form !== form || password.form !== form ||
        !password.isConnected || !editable(password) ||
        password.type !== "password" || password.autocomplete === "new-password" ||
        /otp|one.?time|verification|captcha|new.?password|confirm|验证码|动态|新密码|确认密码/i.test(description(password)) ||
        !safeForm(password.form) ||
        username.value !== request.username || password.value) return {status: "form-changed"};
    setValue(password, request.password);
    return {status: "filled"};
})
